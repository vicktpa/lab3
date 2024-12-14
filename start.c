#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void    checkArg(const char *a)
{
  while (*a)
    {
      if (   (*a == ';')
          || (*a == '&')
          || (*a == '|')
          || (*a == ',')
          || (*a == '$')
          || (*a == '(')
          || (*a == ')')
          || (*a == '{')
          || (*a == '}')
          || (*a == '\`')
          || (*a == '>')
          || (*a == '<') ) {
        puts("Forbidden !!!");
        exit(2);
      }
        a++;
    }
}

int     main()
{
  char  *arg = malloc(0x20);
  char  *cmd = malloc(0x400);
  setreuid(geteuid(), geteuid());

  strcpy(cmd, "/bin/ls -l ");

  printf("Enter directory you want to display : ");

  gets(arg);
  checkArg(arg);

  strcat(cmd, arg);
  system(cmd);

  return 0;
}
