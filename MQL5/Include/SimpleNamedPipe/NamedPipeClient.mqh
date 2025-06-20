//+------------------------------------------------------------------+
//|                                              NamedPipeClient.mqh |
//|                                      Copyright 2025, NewYaroslav |
//|                   https://github.com/NewYaroslav/SimpleNamedPipe |
//+------------------------------------------------------------------+

/// \file NamedPipeClient.mqh
/// \brief Implementation of NamedPipeClient class for working with Windows Named Pipes.

#ifndef _SIMPLE_NAMED_PIPE_CLIENT_MQH
#define _SIMPLE_NAMED_PIPE_CLIENT_MQH

#property copyright "Copyright 2025, NewYaroslav"
#property link      "https://github.com/NewYaroslav/SimpleNamedPipe"

#include <WinAPI\winapi.mqh>

/// \def SNP_ENABLE_GLOBAL_CALLBACKS
/// \brief Enables global callback usage for NamedPipeClient events.
///
/// If defined, the following macros will call user-defined global functions:
/// - `SNP_CALL_ON_OPEN(client)` → `OnPipeOpen(client)`
/// - `SNP_CALL_ON_CLOSE(client)` → `OnPipeClose(client)`
/// - `SNP_CALL_ON_ERROR(client, msg)` → `OnPipeError(client, msg)`
/// - `SNP_CALL_ON_MESSAGE(client, msg)` → `OnPipeMessage(client, msg)`
///
/// If not defined, all `SNP_CALL_ON_*` macros expand to no-ops.
// #define SNP_ENABLE_GLOBAL_CALLBACKS
#ifdef SNP_ENABLE_GLOBAL_CALLBACKS

#define SNP_CALL_ON_OPEN(client)         OnPipeOpen(client)
#define SNP_CALL_ON_CLOSE(client)        OnPipeClose(client)
#define SNP_CALL_ON_ERROR(client, msg)   OnPipeError(client, msg)
#define SNP_CALL_ON_MESSAGE(client, msg) OnPipeMessage(client, msg)

#else

#define SNP_CALL_ON_OPEN(client)
#define SNP_CALL_ON_CLOSE(client)
#define SNP_CALL_ON_ERROR(client, msg)
#define SNP_CALL_ON_MESSAGE(client, msg)

#endif

#define PIPE_READMODE_MESSAGE       2
#define PIPE_WAIT_TIMEOUT_MS        5000
#define INVALID_HANDLE_VALUE        -1
#define GENERIC_READ                0x80000000
#define GENERIC_WRITE               0x40000000
#define OPEN_EXISTING               3
#define PIPE_UNLIMITED_INSTANCES    255
#define PIPE_BUFFER_SIZE            4096
#define STR_SIZE                    255

//+------------------------------------------------------------------+
//| DLL imports                                                      |
//+------------------------------------------------------------------+
#import "kernel32.dll"
ulong CreateNamedPipe(string pipeName,int openMode,int pipeMode,int maxInstances,int outbuffer_size,int inbuffer_size,int defaultTimeOut,int security);
int   WaitNamedPipeW(string lpNamedPipeName,int nTimeOut);
bool  SetNamedPipeHandleState(HANDLE fileHandle,uint &lpMode, uint lpMaxCollectionCount, uint lpCollectDataTimeout);
int   WriteFile(HANDLE file,uchar &buffer[],uint number_of_bytes_to_write,uint &number_of_bytes_written,PVOID overlapped);
int   ReadFile(HANDLE file,char &buffer[],uint number_of_bytes_to_read,uint &number_of_bytes_read,PVOID overlapped);
bool  PeekNamedPipe(HANDLE fileHandle, int buffer, int bytes, int bytesRead, int &numOfBytes, int bytesLeftThisMessage);
#import

//+------------------------------------------------------------------+
//| Interface for event handler                                      |
//+------------------------------------------------------------------+

/// \brief Interface for handling NamedPipeClient events
class INamedPipeHandler {
public:
   virtual void on_open() {}
   virtual void on_close() {}
   virtual void on_message(const string &message) {}
   virtual void on_error(const string &error_message) {}
};

//+------------------------------------------------------------------+
//| NamedPipeClient class                                            |
//+------------------------------------------------------------------+

/// \brief Client for connecting to named pipes and exchanging messages
class NamedPipeClient {
public:

    /// \brief Default constructor
    NamedPipeClient() {
        init_members();
    }
    
    /// \brief Constructor with pipe name
    /// \param name Pipe name to connect to
    NamedPipeClient(const string &name) {
        pipe_name_prefix  = "\\\\.\\pipe\\";
        init_members();
        open(name);
    }
    
    /// \brief Destructor, automatically closes pipe
    ~NamedPipeClient() {
        close();
    }
    
    /// \brief Set external event handler
    /// \param h Pointer to event handler object
    void set_handler(INamedPipeHandler *h) {
        handler = h;
    }

    /// \brief Check if client is connected
    /// \return True if connected
    bool connected() {
        return is_connected;
    }
    
    /// \brief Set internal buffer size
    /// \param size Buffer size in bytes
    void set_buffer_size(int size) {
       buffer_size = size;
    }

    /// \brief Open named pipe
    /// \param name Name of the pipe
    /// \return True if successful, false otherwise
    bool open(const string &name) {
        if (pipe_handle != INVALID_HANDLE_VALUE) {
            close();
        }

        pipe_name = name;
        const string full_pipe_name = pipe_name_prefix + pipe_name;

        if (WaitNamedPipeW(full_pipe_name, PIPE_WAIT_TIMEOUT_MS) == 0) {
            const string err_msg = pipe_name_error_title + "Pipe " + full_pipe_name + " busy.";
            if (!is_error) { 
                if (handler != NULL) handler.on_error(err_msg);
                SNP_CALL_ON_ERROR(GetPointer(this), err_msg);
            }
            is_error = true;
            return false;
        }

        pipe_handle = CreateFileW(
            full_pipe_name, 
            (int)(GENERIC_READ | GENERIC_WRITE), 
            0, 
            NULL, 
            OPEN_EXISTING, 
            0, 
            NULL);

        if (pipe_handle == INVALID_HANDLE_VALUE) {
            const string err_msg= pipe_name_error_title + "Pipe open failed, error: " + IntegerToString(kernel32::GetLastError());
            if (!is_error) { 
                if (handler != NULL) handler.on_error(err_msg);
                SNP_CALL_ON_ERROR(GetPointer(this), err_msg);
            }
            is_error = true;
            return false;
        }
        
        // Set read mode.
        // The client side of a named pipe starts in byte mode,
        // even if the server is configured in message mode.
        // To avoid issues with reading data,
        // set the client side to message mode as well.
        // To change the pipe mode, the client must open the pipe
        // with GENERIC_READ and FILE_WRITE_ATTRIBUTES access.
        uint mode = PIPE_READMODE_MESSAGE;
        bool success = SetNamedPipeHandleState(
            pipe_handle,
            mode,
            NULL,     // do not set max bytes
            NULL);    // do not set max time
     
        if(!success) {
            const string err_msg = pipe_name_error_title + "SetNamedPipeHandleState failed, error: " + IntegerToString(kernel32::GetLastError());
            if (!is_error) { 
                if (handler != NULL) handler.on_error(err_msg);
                SNP_CALL_ON_ERROR(GetPointer(this), err_msg);
            }
            is_error = true;
            CloseHandle(pipe_handle);
            pipe_handle = INVALID_HANDLE_VALUE;
            return false;
        }

        if (handler != NULL) handler.on_open();
        SNP_CALL_ON_OPEN(GetPointer(this));

        is_connected = true;
        is_error = false;
        return true;
    }

    /// \brief Close pipe handle and notify if previously connected
    /// \return 0 if successful, non-zero otherwise
    int close() {
        if (pipe_handle == INVALID_HANDLE_VALUE) return 0;

        int err = CloseHandle(pipe_handle);
        pipe_handle = INVALID_HANDLE_VALUE;
        
        if (is_connected) {
            if (handler != NULL) handler.on_close();
            SNP_CALL_ON_CLOSE(GetPointer(this));
        }
        is_connected = false;
        return err;
    }

    /// \brief Flush pipe output buffer
    void flush() {
        if (pipe_handle == INVALID_HANDLE_VALUE) return;
        FlushFileBuffers(pipe_handle);
    }
    
    /// \brief Write UTF-8 encoded message to pipe
    /// \param message String to send
    /// \return True if write was successful
    bool write(const string &message) {
        if (pipe_handle == INVALID_HANDLE_VALUE) return false;
        if (StringLen(message) == 0) return false;
        
        uchar utf8_array[];
        uint bytes_written = 0;
        uint bytes_to_write = StringToCharArray(message, utf8_array, 0, -1, CP_UTF8);
        bytes_to_write = ArraySize(utf8_array);
        bytes_to_write--; // При копировании функция StringToCharArray() также копирует символ '\0', завершающий строку.

        bool result = WriteFile(pipe_handle, utf8_array, bytes_to_write, bytes_written, NULL);
        
        if (!result || bytes_written != bytes_to_write) {
            const string err_msg = pipe_name_error_title + "WriteFile failed, error: " + IntegerToString(kernel32::GetLastError());
            if (handler != NULL) handler.on_error(err_msg);
            SNP_CALL_ON_ERROR(GetPointer(this), err_msg);
            close();
            return false;
        }

        return true;
    }

    /// \brief Read message from pipe
    /// \return UTF-8 decoded string or empty if failed
    string read() {
        if (pipe_handle == INVALID_HANDLE_VALUE) return "";
        char char_array[];
        ArrayResize(char_array, buffer_size);
        
        uint bytes_read = 0;
        bool result = ReadFile(
            pipe_handle,
            char_array,
            buffer_size,
            bytes_read,
            NULL);
            
        if (!result || bytes_read == 0) {
            const string err_msg = pipe_name_error_title + "ReadFile failed, error: " + IntegerToString(kernel32::GetLastError());
            if (handler != NULL) handler.on_error(err_msg);
            SNP_CALL_ON_ERROR(GetPointer(this), err_msg);
            close();
            return "";
        }

        return CharArrayToString(char_array, 0, bytes_read, CP_UTF8);
    }
    
    /// \brief Update connection and process any incoming messages
    void update() {
        if (is_connected) {
            if (get_bytes_read() > 0) {
                const string message = read();
                if (handler != NULL) handler.on_message(message);
                SNP_CALL_ON_MESSAGE(GetPointer(this), message);
            }
        } else {
            open(pipe_name);
        }
    }
    
    /// \brief Check how many bytes are available to read
    /// \return Number of bytes available
    int get_bytes_read() {
        if (pipe_handle == INVALID_HANDLE_VALUE) return 0;
        int bytes_to_read = 0;
        PeekNamedPipe(
            pipe_handle,
            NULL,
            0,
            NULL,
            bytes_to_read,
            NULL);
        return bytes_to_read;
    }

    /// \brief Get pipe name
    /// \return Pipe name as string
    string get_pipe_name() {
        return pipe_name;
    }
    
    /// \brief Get pipe handle
    /// \return Pipe HANDLE
    HANDLE get_pipe_handle() {
        return pipe_handle;
    }

    /// \brief Check if last operation caused an error
    /// \return True if error occurred
    bool has_error() const { return is_error; }
    
private:
    INamedPipeHandler *handler;
    HANDLE  pipe_handle;
    string  pipe_name_error_title;
    string  pipe_name;
    string  pipe_name_prefix;
    uint    buffer_size;
    bool    is_connected;
    bool    is_error;

    void init_members() {
        pipe_name_prefix  = "\\\\.\\pipe\\";
        pipe_name_error_title = "NamedPipeClient error! What: ";
        buffer_size  = PIPE_BUFFER_SIZE;
        pipe_handle  = INVALID_HANDLE_VALUE;
        is_connected = false;
        is_error     = false;
        handler    = NULL;
        kernel32::GetLastError();
    }
};

#endif // _SIMPLE_NAMED_PIPE_CLIENT_MQH
