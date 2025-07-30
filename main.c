#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_ARGV 256
#define MAX_CMDS 8
#define MAX_USER_PROMPT 4096
#define MAX_JOBS 8
#define PROMPT "> "
#define SEPARATOR " \t\n"

#define STATE_DEFAULT 0
#define STATE_STOPPED 1
#define STATE_FG 2
#define STATE_BG 3

struct job_t
{
  pid_t pgid;
  char state;
  char prompt[MAX_USER_PROMPT];
};

struct job_t jobs[MAX_JOBS] = {0};
char *stateNames[4] = {"DEFAULT", "STOPPED", "FG", "BG"};

// Get PGID of current foreground job
pid_t getFgpg()
{
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (jobs[i].state == STATE_FG)
      return jobs[i].pgid;
  }
  return 0;
}

// SIGINT handler (Ctrl+C)
void sigintHandler(int sig)
{
  pid_t fgpg = getFgpg();
  if (fgpg > 0)
    killpg(fgpg, SIGINT);
}

// SIGTSTP handler (Ctrl+Z)
void sigtstpHandler(int sig)
{
  pid_t fgpg = getFgpg();
  if (fgpg > 0)
    killpg(fgpg, SIGTSTP);
}

// SIGCHLD handler: update job states
void sigchldHandler(int sig)
{
  int status;
  pid_t childpid;
  // Waitpid will run once on every child that changing state,
  // it will not block on a child that is not changing state.
  while ((childpid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
  {
    for (int i = 0; i < MAX_JOBS; i++)
    {
      if (jobs[i].pgid == childpid)
      {
        if (WIFSTOPPED(status))
          jobs[i].state = STATE_STOPPED;
        // Remove the job from the jobs
        else if (WIFEXITED(status) || WIFSIGNALED(status))
          jobs[i].state = STATE_DEFAULT;
        break;
      }
    }
  }
}

// Print all active jobs
void printJobs()
{
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (jobs[i].state != STATE_DEFAULT)
      printf("[%d] pgid=%d  state=%s  cmd=%s",
             i, jobs[i].pgid, stateNames[jobs[i].state], jobs[i].prompt);
  }
}

int main()
{
  signal(SIGINT, sigintHandler);
  signal(SIGTSTP, sigtstpHandler);
  signal(SIGCHLD, sigchldHandler);

  // Holds the prompt that will may
  // become a command (after tokenising)
  char userPrompt[MAX_USER_PROMPT];
  // To track the changes (because strtok mutates the userPrompt)
  char promptCopy[MAX_USER_PROMPT];
  char *cmds[MAX_CMDS][MAX_ARGV];

  while (1)
  {
    printf(PROMPT);
    fflush(stdout);

    if (!fgets(userPrompt, MAX_USER_PROMPT, stdin))
      break;
    if (feof(stdin))
      exit(0);
    strncpy(promptCopy, userPrompt, MAX_USER_PROMPT);

    // Tokenise input
    // A counter of the pipes (separates cmds)
    int cmdIdx = 0;
    // Holds tokenising logic
    int argIdx = 0;
    char *token = strtok(userPrompt, SEPARATOR);
    memset(cmds, 0, sizeof(cmds));

    while (token)
    {
      if (strcmp(token, "|") == 0)
      {
        cmds[cmdIdx++][argIdx] = NULL;
        argIdx = 0;
      }
      else
      {
        cmds[cmdIdx][argIdx++] = token;
      }
      token = strtok(NULL, SEPARATOR);
    }
    cmds[cmdIdx][argIdx] = NULL;

    // Built-in commands
    if (!cmds[0][0])
      continue;

    if (strcmp(cmds[0][0], "quit") == 0)
      exit(0);
    if (strcmp(cmds[0][0], "help") == 0)
    {
      puts("Built-ins: quit, help, jobs, fg <id>, bg <id>");
      continue;
    }
    if (strcmp(cmds[0][0], "jobs") == 0)
    {
      printJobs();
      continue;
    }

    if (strcmp(cmds[0][0], "fg") == 0 || strcmp(cmds[0][0], "bg") == 0)
    {
      if (!cmds[0][1])
      {
        printf("Usage: %s <jobId>\n", cmds[0][0]);
        continue;
      }
      int id = atoi(cmds[0][1]);
      if (id < 0 || id >= MAX_JOBS || jobs[id].state == STATE_DEFAULT)
      {
        printf("Invalid job id\n");
        continue;
      }
      killpg(jobs[id].pgid, SIGCONT);
      jobs[id].state = strcmp(cmds[0][0], "fg") == 0 ? STATE_FG : STATE_BG;
      if (jobs[id].state == STATE_FG)
        while (jobs[id].state == STATE_FG)
          sleep(1);
      continue;
    }

    // Execute pipeline
    int fds[2], infd = 0, jobId = -1;
    pid_t pids[MAX_CMDS];

    for (int i = 0; i <= cmdIdx; i++)
    {
      if (i != cmdIdx)
        pipe(fds);

      if ((pids[i] = fork()) == 0)
      {
        if (i != cmdIdx)
        {
          dup2(fds[1], STDOUT_FILENO);
          close(fds[0]);
          close(fds[1]);
        }
        if (infd)
        {
          dup2(infd, STDIN_FILENO);
          close(infd);
        }
        execvp(cmds[i][0], cmds[i]);
        perror("exec failed");
        exit(1);
      }
      else
      {
        if (i == 0)
        {
          for (jobId = 0; jobId < MAX_JOBS; jobId++)
          {
            if (jobs[jobId].state == STATE_DEFAULT)
            {
              jobs[jobId].pgid = pids[0];
              jobs[jobId].state = STATE_FG;
              strncpy(jobs[jobId].prompt, promptCopy, MAX_USER_PROMPT);
              break;
            }
          }
        }
        setpgid(pids[i], pids[0]);
        if (i != cmdIdx)
        {
          close(fds[1]);
          infd = fds[0];
        }
      }
    }

    if (jobId >= 0)
    {
      while (jobs[jobId].state == STATE_FG)
        sleep(1);
    }
  }

  return 0;
}
