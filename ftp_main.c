/* FileName:ftp_main.c;
 *
 * Author:snownight;
 * Date:2018.04.11;
 *
 */
#include <stdio.h>
#include "ftp_err.h"

int main(int argc, char *argv[])
{

    ftp_ctrl_proc(NULL, 11);//"192.168.1.186", 21);
    return FTP_OK;
}
