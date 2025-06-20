//+------------------------------------------------------------------+
//|                                 NamedPipeClientWithCallbacks.mq5 |
//|                                     Copyright 2025, NewYaroslav. |
//|                   https://github.com/NewYaroslav/SimpleNamedPipe |
//+------------------------------------------------------------------+
#define SNP_ENABLE_GLOBAL_CALLBACKS
#include <SimpleNamedPipe\NamedPipeClient.mqh>

input string pipe_name      = "ExamplePipe"; // Named Pipe for the stream of quotes
input int    timer_period   = 10;            // Timer period for processing incoming messages

NamedPipeClient *pipe = NULL;

//+------------------------------------------------------------------+
//| Callback functions                                               |
//+------------------------------------------------------------------+
void OnPipeOpen(NamedPipeClient *client) {
    Print("Open connection with ", client.get_pipe_name());
}

void OnPipeClose(NamedPipeClient *client) {
    Print("Closed connection with ", client.get_pipe_name());
}

void OnPipeMessage(NamedPipeClient *client, const string &message) {
    Print("Message: " + message);
}

void OnPipeError(NamedPipeClient *client, const string &error_message) {
    Print("Error! What: " + error_message);
}

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit() {
    if (pipe == NULL) {
        if ((pipe = new NamedPipeClient(pipe_name)) == NULL) {
            return(false);
        }
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
    if (pipe != NULL) {
        pipe.update();
    }
    
    ticks += timer_period;
    if (ticks >= 1000) {
        ticks = 0;
        if (pipe != NULL && pipe.connected()) {
            pipe.write("ping");
        }
    }
}
//+------------------------------------------------------------------+
