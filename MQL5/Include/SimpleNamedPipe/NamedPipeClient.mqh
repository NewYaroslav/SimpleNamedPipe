//+------------------------------------------------------------------+
//|                                              NamedPipeClient.mqh |
//|                                      Copyright 2025, NewYaroslav |
//|                   https://github.com/NewYaroslav/SimpleNamedPipe |
//+------------------------------------------------------------------+
#property copyright "Copyright 2025, NewYaroslav"
#property link      "https://github.com/NewYaroslav/SimpleNamedPipe"

#include <WinAPI\winapi.mqh>

#define PIPE_READMODE_MESSAGE       2

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

/// \brief Класс клиента именованного канала
class NamedPipeClient {
private:
	HANDLE  pipe_handle;      // хэндл канала
	string  pipe_name_error_title;
	string  pipe_name;
	string  pipe_name_prefix;
	uint    buffer_size;
	int     pipe_id;
    bool    is_connected;
    bool    is_error;
public:

        NamedPipeClient() {
                pipe_name_prefix  = "\\\\.\\pipe\\";
                pipe_name_error_title = "NamedPipeClient error! What: ";
                buffer_size       = PIPE_BUFFER_SIZE;
                pipe_handle       = INVALID_HANDLE_VALUE;
                is_connected      = false;
                is_error          = false;
                pipe_id = 0;
                kernel32::GetLastError();
                on_open    = NULL;
                on_close   = NULL;
                on_message = NULL;
                on_error   = NULL;
        }
	
	/// \brief Class constructor
	/// \param name      Named channel name
	/// \param user_id   Unique channel number
        NamedPipeClient(const string &name, const int user_id = 0) {
            pipe_name_prefix  = "\\\\.\\pipe\\";
                pipe_name_error_title = "NamedPipeClient error! What: ";
                buffer_size       = PIPE_BUFFER_SIZE;
                pipe_handle       = INVALID_HANDLE_VALUE;
                is_connected      = false;
                is_error          = false;
                pipe_id = 0;
                kernel32::GetLastError();
                on_open    = NULL;
                on_close   = NULL;
                on_message = NULL;
                on_error   = NULL;
                open(name, user_id);
        }
	
	/// \brief Class destructor
	~NamedPipeClient() {
		close();
	}
	
        // Callback pointers
        void (*on_open)   (NamedPipeClient *pointer);
        void (*on_close)  (NamedPipeClient *pointer);
        void (*on_message)(NamedPipeClient *pointer, const string &message);
        void (*on_error)  (NamedPipeClient *pointer, const string &error_message);
	
	/// \brief Проверка наличия соединения
	/// \return Вернет true, если соединение установлено
	bool connected() {
	    return is_connected;
	}
	
	/// \brief Установить размер буфера
	/// \param size Размер буфера
	void set_buffer_size(int size) {
	   buffer_size = size;
	}

	/// \brief Открывает ранее созданный канал
	/// \param name      Имя именованного канала
	/// \param user_id   Уникальный номер канала
	/// \return Вернет true, если успешно, иначе false
	bool open(const string &name, const int user_id = 0) {
		if(pipe_handle == INVALID_HANDLE_VALUE) {
			pipe_name = name;
		    pipe_id = user_id;
		    const string full_pipe_name = pipe_name_prefix + pipe_name;
 
                        if(WaitNamedPipeW(full_pipe_name, 5000) == 0) {
                                const string err_msg = pipe_name_error_title + "Pipe " + full_pipe_name + " busy.";
                                if (!is_error && on_error != NULL) on_error(GetPointer(this), err_msg);
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
                        if(pipe_handle == INVALID_HANDLE_VALUE) {
                                const string err_msg= pipe_name_error_title + "Pipe open failed, error: " + IntegerToString(kernel32::GetLastError());
                                if (!is_error && on_error != NULL) on_error(GetPointer(this), err_msg);
                                is_error = true;
                                return false;
                        }
			
			/* устанавливаем режим чтения
            * Клиентская сторона именованного канала начинается в байтовом режиме,
            * даже если серверная часть находится в режиме сообщений.
            * Чтобы избежать проблем с получением данных,
            * установите на стороне клиента также режим сообщений.
            * Чтобы изменить режим канала, клиент канала должен
            * открыть канал только для чтения с доступом
            * GENERIC_READ и FILE_WRITE_ATTRIBUTES
            */
            uint mode = PIPE_READMODE_MESSAGE;
            bool success = SetNamedPipeHandleState(
                pipe_handle,
                mode,
                NULL,     // не устанавливать максимальные байты
                NULL);    // не устанавливайте максимальное время
         
            if(!success) {
                const string err_msg = pipe_name_error_title + "SetNamedPipeHandleState failed, error: " + IntegerToString(kernel32::GetLastError());
                if (!is_error && on_error != NULL) on_error(GetPointer(this), err_msg);
                is_error = true;
                CloseHandle(pipe_handle);
                pipe_handle = INVALID_HANDLE_VALUE;
                return false;
            }
            if(on_open != NULL) on_open(GetPointer(this));
		}
		is_connected = true;
		is_error = false;
		return true;
	}

	/// \brief Закрывает дескриптор канала
	/// \return 0, если успешно, иначе ненулевое значение
	int close() {
		if (pipe_handle != INVALID_HANDLE_VALUE) {
                int err = CloseHandle(pipe_handle);
                pipe_handle = INVALID_HANDLE_VALUE;
                if (is_connected && on_close != NULL) on_close(GetPointer(this));
                is_connected = false;
    		return err;
		}
		return 0;
	}

	/// \brief Сбрасывает буфер канала
	void flush() {
        if (pipe_handle == INVALID_HANDLE_VALUE) return;
        FlushFileBuffers(pipe_handle);
	}
	
	/// \brief Write a string to a pipe
	/// \param message String containing the message
	/// \return Returns true if the write was successful
	bool write(string message) {
		if (pipe_handle == INVALID_HANDLE_VALUE) return false;
        if (StringLen(message) == 0) return false;
        uchar utf8_array[];
        uint bytes_written = 0;
        uint bytes_to_write = StringToCharArray(message, utf8_array, 0, -1, CP_UTF8);
        bytes_to_write = ArraySize(utf8_array);
        bytes_to_write--; // При копировании функция StringToCharArray() также копирует символ '\0', завершающий строку.
        WriteFile(
			pipe_handle,
			utf8_array,
			bytes_to_write,
			bytes_written,
			NULL);
		Print("bytes_to_write: ", bytes_to_write);
        if(bytes_written != bytes_to_write) {
            close();
            return false;
        }
        return true;
	}
   
	/// \brief Читает строку формата ANSI из канала
	/// \return Строка в формате Unicode (string в MQL5)
	string read() {
        if (pipe_handle == INVALID_HANDLE_VALUE) return "";
        string ret;
        char char_array[];
        ArrayResize(char_array, buffer_size);
        uint bytes_read = 0;
        ReadFile(
            pipe_handle,
            char_array,
            buffer_size,
            bytes_read,
            0);
        if(bytes_read != 0) ret = CharArrayToString(char_array, 0, bytes_read, CP_UTF8);
        return ret;
	}
	
        void on_timer() {
        if (is_connected) {
            if (get_bytes_read() > 0) {
                const string message = read();
                if(on_message != NULL) on_message(GetPointer(this), message);
            }
        } else {
            open(pipe_name, pipe_id);
        }
        }
	
	/// \brief Получить количество байтов для чтения
	/// \return Количество байтов для чтения
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
	};

	/// \brief Возвращает имя канала
	/// \return Строка с именем канала
	string get_pipe_name() {
		return pipe_name;
	};
	
	/// \brief Возвращает handle канала
	/// \return HANDLE канала
    HANDLE get_pipe_handle() {
        return pipe_handle;
    };
    
	/// \brief Возвращает user_id канала
	/// \return user_id канала
    int get_pipe_id() {
        return pipe_id;
    };
};
