#ifndef _FTP_CTRL_H_
#define _FTP_CTRL_H_
#include <pthread.h>

#define FTP_COMMAND_PROMPT    "Ftp>>"

#define FTP_OK    (0)
#define FTP_ERR    (-1)

#define FTP_REPLY_FLAG_ONE    (1<<1)
#define FTP_REPLY_FLAG_TWO    (1<<2)
#define FTP_REPLY_FLAG_THREE    (1<<3)
#define FTP_REPLY_FLAG_FOUR    (1<<4)
#define FTP_REPLY_FLAG_FIVE    (1<<5)

#define FTP_REPLY_FLAG_FAIL   FTP_REPLY_FLAG_FOUR | FTP_REPLY_FLAG_FIVE 

typedef struct ftpCmd
{
    char* command;
    char* ftpcommand;
    unsigned char successflag;
    unsigned char failflag;
    unsigned char waitflag;
    unsigned char nextcommandflag;
    unsigned char toconnect;
    unsigned char hasarg;
    unsigned char needInput;
    unsigned char argindex;
    unsigned char usedata;
    unsigned char isFtpCmd;
    unsigned char saveData;
    unsigned char upData;
    char* printBefore;
    char* printAfter;
    /*
    unsigned char hasArg;
    char* defaultValue;
    char* printStrBefore;
    char* printStrAfter;
    unsigned char ispwd;
    */
}stFtpCmd;

typedef struct ftpContext
{
    char domain[128];
    unsigned short port;
    unsigned char ispassive;
    unsigned char finishpasv;
    unsigned char argnums;
    char *args;
    void *ctrlHandler;
    void *dataHandler;
    FILE *datafp;
    void *dataClient;
    stFtpCmd *currentCmd;
    pthread_mutex_t lock;
    pthread_cond_t cond;
}stFtpContext;

#endif
