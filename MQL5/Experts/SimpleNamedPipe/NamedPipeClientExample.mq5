//+------------------------------------------------------------------+
//|                                       NamedPipeClientExample.mq5 |
//|                                     Copyright 2025, NewYaroslav. |
//|                   https://github.com/NewYaroslav/SimpleNamedPipe |
//+------------------------------------------------------------------+
#include "SimpleNamedPipe\NamedPipeClient.mqh"

input string pipe_name      = "ExamplePipe"; // Named Pipe for the stream of quotes
input int    timer_period   = 10;            // Timer period for processing incoming messages

NamedPipeClient     *pipe           = NULL;

//+------------------------------------------------------------------+
//| NamedPipeClient function                                   |
//+------------------------------------------------------------------+
void NamedPipeClient::on_open(NamedPipeClient *pointer) {
    Print("open connection with ", pointer.get_pipe_name());
}

void NamedPipeClient::on_close(NamedPipeClient *pointer) {
    Print("closed connection with ", pointer.get_pipe_name());
}

void NamedPipeClient::on_message(NamedPipeClient *pointer, const string &message) {
    Print("message: " + message);
}

void NamedPipeClient::on_error(NamedPipeClient *pointer, const string &error_message) {
    Print("Error! What: " + error_message);
}

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit() {
    if (pipe == NULL) {
        if ((pipe = new NamedPipeClient(pipe_name)) == NULL) return(false);
    }

    EventSetMillisecondTimer(timer_period);
    return(INIT_SUCCEEDED);
}
//+------------------------------------------------------------------+
//| Expert deinitialization function                                 |
//+------------------------------------------------------------------+
void OnDeinit(const int reason) {
    EventKillTimer();
    if (pipe != NULL) delete pipe;
}
//+------------------------------------------------------------------+
//| Expert tick function                                             |
//+------------------------------------------------------------------+
void OnTick() {

}
//+------------------------------------------------------------------+
//| Timer function                                                   |
//+------------------------------------------------------------------+
void OnTimer() {
    static int ticks = 0;
    if (pipe != NULL) pipe.on_timer();
    
    ticks += timer_period;
    if (ticks >= 1000) {
        ticks = 0;

        if (pipe != NULL) {
            if (pipe.connected()) {
                const string str_ping = "ping";
                pipe.write(str_ping);
            } // if (pipe.connected())
        } // if (pipe != NULL)
    } // if (ticks >= 1000)
}
//+------------------------------------------------------------------+
