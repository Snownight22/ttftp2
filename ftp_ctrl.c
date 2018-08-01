#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tcp_session.h"
#include "log.h"
#include "ftp_ctrl.h"

static stFtpCmd g_ctrl_commands[] = 
{
    {"open", NULL, FTP_REPLY_FLAG_TWO, 0, 0, FTP_REPLY_FLAG_TWO, 1, 0, 0, 0, 0, 1, 0, NULL, NULL},
    {"user", "USER", FTP_REPLY_FLAG_TWO, FTP_REPLY_FLAG_FAIL, 0, FTP_REPLY_FLAG_THREE, 0, 1, 1, 0, 0, 1, 0,"Name (192.168.1.186: anonymous as default):", NULL},
    {"password", "PASS", FTP_REPLY_FLAG_TWO, FTP_REPLY_FLAG_FAIL, 0, FTP_REPLY_FLAG_THREE, 0, 1, 1, 0, 0, 1, 0, "Password:", NULL},
    {"account", "ACCT", FTP_REPLY_FLAG_TWO, FTP_REPLY_FLAG_FAIL, 0, 0, 0, 1, 0, 0, 0, 1, 0, NULL, NULL},
    {"system", "SYST", FTP_REPLY_FLAG_TWO, FTP_REPLY_FLAG_FAIL, 0, 0, 0, 0, 0, 0, 0, 1, 0, NULL, NULL},
    {"ls", "LIST", FTP_REPLY_FLAG_TWO, FTP_REPLY_FLAG_FAIL, FTP_REPLY_FLAG_ONE, 0, 0, 0, 0, 0, 1, 1, 0, NULL, NULL},
    {"passive", "PASV", FTP_REPLY_FLAG_TWO, FTP_REPLY_FLAG_FAIL, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL},
    {"get", "RETR", FTP_REPLY_FLAG_TWO, FTP_REPLY_FLAG_FAIL, FTP_REPLY_FLAG_ONE, 0, 0, 1, 0, 0, 1, 1, 1, NULL, NULL},
};

static stFtpContext *g_ftp_context;

static int command_analysis(char* string, char* command, char* args, int* argsnum)
{
    int length = strlen(string);
    int i = 0;
    char *p;

    while (isblank(string[i]) && i < length)
        i++;

    p = &string[i];
    *argsnum = -1;
    while (i <= length)
    {
        if (isblank(string[i]) || string[i] == 0)
        {
            string[i] = '\0';
            if (*argsnum == -1)
                strcpy(command, p);
            else
                strcpy(args + (*argsnum) * 128, p);
            (*argsnum)++;
            i++;

            while (isblank(string[i]) && i < length)
                i++;
            p = &string[i];
        }
        else
            i++;
    }

    return FTP_OK;
}


static int reply_analysis(char *reply, int *errcode)
{
    int i = 0;
    int length = strlen(reply);
    char code[4] = {0};
    char *p = reply;

    while (isblank(*p))
        p++;

    strncpy(code, p, 3);
    *errcode = atoi(code);

    return FTP_OK;
}

stFtpContext* ftp_ctrl_context_init(char* domain, unsigned short port)
{
    stFtpContext *ftpC;

    if (NULL == (ftpC = (stFtpContext *) malloc(sizeof(stFtpContext))))
        return NULL;

    memset(ftpC, 0, sizeof(stFtpContext));
    if (domain)
        strcpy(ftpC->domain, domain);
    if (port == 0)
        ftpC->port = 21;
    //ftpC->port = port;
    pthread_mutex_init(&ftpC->lock, NULL);
    pthread_cond_init(&ftpC->cond, NULL);

    return ftpC;
}

void ftp_ctrl_context_destory(stFtpContext *ftpC)
{
    if (NULL != ftpC)
    {
        if (NULL != ftpC->ctrlHandler)
        {
            tcp_client_destroy(ftpC->ctrlHandler);
            ftpC->ctrlHandler = NULL;
        }
        free(ftpC);
    }
}

void ftp_ctrl_data_reply(const void *handler, const void *data, const int length)
{
    stFtpContext *context = g_ftp_context;
    stFtpCmd *cmd = context->currentCmd;
    char fileName[128] = {0};

    if (cmd->saveData)
    {
        FILE *fp;
        strcpy(fileName, context->args + cmd->argindex * 128);
        fp = fopen(fileName, "wb");
        if (NULL == fp)
        {
            LOG_ERROR("can't save file, open error\n");
            return ;
        }
        fwrite(data, length, 1, fp);
        fclose(fp);
    }
    else
        fprintf(stdout, "%s\n", data);
}

int ftp_ctrl_getlocalinfo(stFtpContext *context, long *lip, unsigned short *lport)
{
    long ip;
    unsigned short port;

    tcp_client_addr_get(context->ctrlHandler, lip, &port);
    tcp_server_addr_get(context->dataHandler, &ip, lport);
    return FTP_OK;
}

int ftp_ctrl_dataserver_init(stFtpContext *context, long *lip, unsigned short *lport)
{
    context->dataHandler = tcp_server_init(NULL, 0, ftp_ctrl_data_reply, NULL);
    if (NULL == context->dataHandler)
        return FTP_ERR;
    ftp_ctrl_getlocalinfo(context, lip, lport);

    return FTP_OK;
}

int ftp_ctrl_dataserver_destory(stFtpContext *context)
{
    if (NULL != context->dataHandler)
    {
        tcp_server_destroy(context->dataHandler);
        context->dataHandler = NULL;
    }

    return FTP_OK;
}

int ftp_ctrl_dataclient_init(stFtpContext *context, char *sip, unsigned short sport)
{
    context->dataHandler = tcp_client_init(sip, sport, ftp_ctrl_data_reply);
    if (NULL == context->dataHandler)
    {
        LOG_ERROR("client DataHandler init error\n");
        return FTP_ERR;
    }
    tcp_client_reconnectTime_set(context->dataHandler, 0);

    return FTP_OK;
}

int ftp_ctrl_dataclient_destroy(stFtpContext *context)
{
    if (NULL != context->dataHandler)
    {
        tcp_client_close_session(context->dataHandler);
        tcp_client_destroy(context->dataHandler);
        context->dataHandler = NULL;
    }
    return FTP_OK;
}

int ftp_ctrl_pasvmsg(char *reply, int *errcode, long *sip, unsigned short *sport) 
{
    char *cp;
    unsigned int v[6];

    cp = strchr(reply, '(');
    if (NULL == cp)
        return FTP_ERR;
    cp++;
    
    sscanf(cp, "%u,%u,%u,%u,%u,%u", &v[2], &v[3], &v[4], &v[5], &v[0], &v[1]);
    *sip = (v[2] << 24) + (v[3] << 16) + (v[4] << 8) + v[5];
    *sport = (v[0] << 8) + v[1];
    
    return FTP_OK;
}

int ftp_ctrl_passive_proc(stFtpContext *context, char *data)
{
    int errcode;
    long sip;
    unsigned short sport;
    char fip[16] = {0};

    ftp_ctrl_pasvmsg(data, &errcode, &sip, &sport);
    snprintf(fip, 15, "%u.%u.%u.%u", (unsigned int)(sip >> 24)&0xff, (unsigned int)(sip >> 16)&0xff, (unsigned int)(sip >> 8)&0xff, (unsigned int)(sip&0xff));
    if (FTP_OK != ftp_ctrl_dataclient_init(context, fip, sport))
        return FTP_ERR;
    return FTP_OK;
}

void ftp_ctrl_notice()
{
    pthread_mutex_lock(&g_ftp_context->lock);
    pthread_cond_signal(&g_ftp_context->cond);
    pthread_mutex_unlock(&g_ftp_context->lock);
}

void ftp_ctrl_reply(const void *handler, const void *data, const int length)
{
    int errorcode = 400;
    stFtpCmd *cmd = g_ftp_context->currentCmd;
    char command[128] = {0};
    char arg[128] = {0};

    reply_analysis((char *)data, &errorcode);
    int code = errorcode / 100;

    ((char *)data)[length] = 0;
    fprintf(stdout, "%s\n", data);
    LOG_INFO("%s\n", data);

    if (cmd->usedata && (0 == g_ftp_context->finishpasv))
    {
        g_ftp_context->finishpasv = 1;
        if (((0x1 << code) & FTP_REPLY_FLAG_TWO) == 0)
        {
            ftp_ctrl_notice();
            return ;
        }

        if (g_ftp_context->ispassive)
        {
            if (FTP_OK != ftp_ctrl_passive_proc(g_ftp_context, (char *)data))
            {
                ftp_ctrl_notice();
                LOG_ERROR("passive proc error\n");
                return ;
            }
        }

        if (cmd->hasarg)
        {
            strcpy(arg, g_ftp_context->args + cmd->argindex * 128);
            snprintf(command, 127, "%s %s\r\n", cmd->ftpcommand, arg);
        }
        else
            snprintf(command, 127, "%s\r\n", cmd->ftpcommand);
        tcp_client_send(g_ftp_context->ctrlHandler, command, strlen(command));
        return ;
    }

    if ((cmd->nextcommandflag & (0x1 << code)) != 0)
    {
        cmd = cmd + 1; //sizeof(stFtpCmd);
        g_ftp_context->currentCmd = cmd;
        if (NULL != cmd->printBefore)
            fprintf(stdout, "%s", cmd->printBefore);
        if (cmd->hasarg)
        {
            if (cmd->needInput)
            {
                fgets(arg, 127, stdin);
                arg[strlen(arg) - 1] = 0;
            }
            else
                strcpy(arg, g_ftp_context->args + cmd->argindex * 128);
            snprintf(command, 127, "%s %s\r\n", cmd->ftpcommand, arg);
        }
        else
            snprintf(command, 127, "%s\r\n", cmd->ftpcommand);

        tcp_client_send(g_ftp_context->ctrlHandler, command, strlen(command));
    }
    else if (((cmd->successflag & (0x1 << code)) != 0) || ((cmd->failflag & (0x1 << code)) != 0))
    {
        if (cmd->usedata)
        {
            if (g_ftp_context->ispassive)
                ftp_ctrl_dataclient_destroy(g_ftp_context);
            else
                ftp_ctrl_dataserver_destory(g_ftp_context);
        }

        pthread_mutex_lock(&g_ftp_context->lock);
        pthread_cond_signal(&g_ftp_context->cond);
        pthread_mutex_unlock(&g_ftp_context->lock);
    }
}

int ftp_ctrl_passive(stFtpContext *context)
{
    char *command = "PASV\r\n";

    tcp_client_send(context->ctrlHandler, command, strlen(command));
    LOG_INFO("command:%s\n", command);
    return FTP_OK;
}

int ftp_ctrl_positive(stFtpContext *context)
{
    long lip;
    unsigned short lport;
    int ret;
    char command[128] = {0};

    ret = ftp_ctrl_dataserver_init(context, &lip, &lport);
    if (FTP_OK != ret)
        return FTP_ERR;
    snprintf(command, 127, "PORT %u,%u,%u,%u,%u,%u\r\n", (unsigned int)(lip >> 24)&0xff, (unsigned int)(lip >> 16)&0xff, (unsigned int)(lip >> 8)&0xff, (unsigned int)(lip)&0xff, (lport >> 8)&0xff, lport & 0xff);
    tcp_client_send(context->ctrlHandler, command, strlen(command));
    LOG_INFO("command:%s\n", command);

    return FTP_OK;
}

int ftp_ctrl_command_logic(stFtpContext *context)
{
    char command[128] = {0};

    if (context->currentCmd->toconnect)
    {
        if (NULL != context->args && context->argnums >= 1)
            strcpy(context->domain, context->args);
        if (NULL != context->args && context->argnums >= 2)
        {
            context->port = atoi(context->args + 128);
        }
        if (context->port == 0)
            context->port = 21;
        context->ctrlHandler = tcp_client_init(context->domain, context->port, ftp_ctrl_reply);
        if (NULL == context->ctrlHandler)
        {
            LOG_ERROR("ftp ctrl tunnel init error\n");
            return FTP_ERR;
        }
        tcp_client_reconnectTime_set(context->ctrlHandler, 0);
    }
    else
    {
        if (NULL == context->ctrlHandler)
        {
            LOG_ERROR("\nInput command error:%s\n", context->currentCmd->command);
            return FTP_ERR;
        }
        if (context->currentCmd->hasarg)
        {
        }
        else
            snprintf(command, 127, "%s\r\n", context->currentCmd->ftpcommand);
        if (context->currentCmd->usedata)
        {
            g_ftp_context->finishpasv = 0;
            if (context->ispassive)
            {
                ftp_ctrl_passive(context);
            }
            else
            {
                ftp_ctrl_positive(context);
            }
        }
        else
            tcp_client_send(context->ctrlHandler, command, strlen(command));
    }
}

int ftp_ctrl_localcmd(stFtpContext *context)
{
    if (!strcmp(context->currentCmd->command, "passive"))
    {
        context->ispassive = 1;
        fprintf(stdout, "Passive Mode\n");
    }

    return FTP_OK;
}

int ftp_ctrl_proc(char* domain, unsigned short port)
{
    stFtpCmd *ctrl;
    char command[128] = {0};
    char input[128] = {0};
    char args[3][128] = {0};
    int argsnum;
    int length;
    int command_len = sizeof(g_ctrl_commands) / sizeof(stFtpCmd);
    int i;

    log_init(NULL);

    if (NULL == (g_ftp_context = ftp_ctrl_context_init(domain, port)))
    {
        LOG_ERROR("ftp context init error\n");
        return FTP_ERR;
    }

    while (1)
    {
        fprintf(stdout, "%s", FTP_COMMAND_PROMPT);
        fgets(input, 127, stdin);
        length = strlen(input);
        input[length - 1] = 0;
        if (length <= 1)
            continue;

        memset(command, 0, sizeof(command));
        command_analysis(input, command, (char *)&args[0], &argsnum);
        if (!strcmp(command, "quit"))
        {
            fprintf(stdout, "byebye\n");
            break;
        }

        g_ftp_context->args = (char *)args;
        g_ftp_context->argnums = argsnum;

        for (i = 0; i < command_len; i++)
        {
            ctrl = &g_ctrl_commands[i];
            if (!strcmp(command, ctrl->command))
            {
                g_ftp_context->currentCmd = ctrl;
                if (ctrl->isFtpCmd)
                {
                    ftp_ctrl_command_logic(g_ftp_context);
                    pthread_mutex_lock(&g_ftp_context->lock);
                    pthread_cond_wait(&g_ftp_context->cond, &g_ftp_context->lock);
                    pthread_mutex_unlock(&g_ftp_context->lock);
                    //fprintf(stdout, "signal receive\n");
                }
                else
                    ftp_ctrl_localcmd(g_ftp_context);
                break;
            }
        }
    }

    ftp_ctrl_context_destory(g_ftp_context);
    g_ftp_context = NULL;

    log_destory();

    return FTP_OK;
}

