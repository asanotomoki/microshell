#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

typedef struct s_cmds
{
	char *cmd;
	char **argv;
	struct s_cmds *piped;
	struct s_cmds *preview;
	struct s_cmds *new;
	int argc;
	int pipe[2];
	pid_t pid;
	bool is_cd;
} t_cmds;

void put_error(char *str)
{
	size_t len = 0;
	while (str[len])
		len++;
	write(2, str, len);
	write(2, "\n", 1);
	exit(1);
}

void exec_error()
{
	put_error("error: cannot execute executable_that_failed");
}

void cd_error()
{
	put_error("error: cd: cannot change directory to path_to_change");
}

void cd_arg_error()
{
	put_error("error: cd: bad arguments");
}

void fatal_error()
{
	put_error("error: fatal");
}

t_cmds *new_cmd(char *cmd)
{
	t_cmds *new;
	new = malloc(sizeof(t_cmds));
	new->cmd = cmd;
	new->argv = malloc(sizeof(char *) * 2);
	new->argv[0] = cmd;
	new->argv[1] = NULL;
	new->piped = NULL;
	new->preview=NULL;
	new->new = NULL;
	new->argc = 1;
	new->pid = 0;
	if (strncmp(cmd, "cd", 2) == 0)
		new->is_cd = true;
	else
		new->is_cd = false;
	return (new);
}

t_cmds *push_back_cmd(t_cmds *cmds, char *cmd)
{
	while (cmds->new)
		cmds = cmds->new;
	cmds->new = new_cmd(cmd);
	return (cmds->new);
}

t_cmds *push_pipe_cmd(t_cmds *cmds, char *cmd)
{
	while (cmds->piped)
		cmds = cmds->piped;
	cmds->piped = new_cmd(cmd);
	cmds->piped->preview = cmds;
	return (cmds->piped);
}

void close_pipe(int pp[2])
{
	close(pp[0]);
	close(pp[1]);
}

char **create_argv(int argc, char **argv, char *new_arg)
{
	char **new;
	new = malloc(sizeof(char *) * (argc + 2));
	int i = 0;
	while (argv[i])
	{
		new[i] = argv[i];
		i++;
	}
	new[i] = new_arg;
	new[i + 1] = NULL;
	//free(argv);

	return (new);
}

int cd(t_cmds *cmds)
{
	if (cmds->argc == 2)
	{
		if (chdir(cmds->argv[1]) == -1)
			cd_error();
	}
	else
		cd_arg_error();
	return (0);
}

t_cmds *create_cmds(char **argv)
{
	t_cmds *cmds;
	cmds = new_cmd(*argv);
	argv++;
	t_cmds *tmp = cmds;
	while (*argv)
	{
		if (strncmp(*argv, "|", 1) == 0)
		{
			argv++;
			tmp = push_pipe_cmd(tmp, *argv);
		}
		else if (strncmp(*argv, ";", 1) == 0)
		{
			argv++;
			tmp = push_back_cmd(cmds, *argv);
		}
		else
		{
			tmp->argv = create_argv(tmp->argc++, tmp->argv, *argv);
		}
		argv++;
	}
	return (cmds);
}

int create_wait_pid(t_cmds *cmd)
{
	t_cmds *tmp;
	int status;
	while (cmd)
	{
		tmp = cmd;
		while (cmd)
		{
			waitpid(cmd->pid, &status, 0);
			cmd = cmd->piped;
		}
		cmd = tmp->new;
	}
	return (WIFEXITED(status));
}

int connect(t_cmds *cmds)
{
	if (!cmds->preview && !cmds->piped)
	{
		return (0);
	}
	if (!cmds->preview) {
		dup2(cmds->pipe[1], 1);
		close_pipe(cmds->pipe);
	} else if (!cmds->piped){
		dup2(cmds->preview->pipe[0], 0);
		close_pipe(cmds->preview->pipe);
	} else if (cmds->preview) {
		dup2(cmds->preview->pipe[0],0);
		dup2(cmds->pipe[1], 1);
		close_pipe(cmds->preview->pipe);
		close_pipe(cmds->pipe);
	}
	return (0);
}

int exec_piped(t_cmds *cmds, char **envp)
{
	while (cmds)
	{
		if (cmds->piped)
			pipe(cmds->pipe);
		cmds->pid = fork();
		if (cmds->pid == 0)
		{
			connect(cmds);
			if (cmds->is_cd)
				cd(cmds);
			else
			{
				execve(cmds->cmd, cmds->argv, envp);
				exit(1);
			}
		} else {
			if (cmds->preview)
				close_pipe(cmds->preview->pipe);
		}
		cmds = cmds->piped;
	}
	return (0);
}

int exec_new(t_cmds *cmds, char **envp)
{
	while (cmds)
	{
		exec_piped(cmds, envp);
		create_wait_pid(cmds);
		cmds = cmds->new;
	}
	return (0);
}

void free_cmds(t_cmds *cmd)
{
	t_cmds *tmp;
	t_cmds *tmp2;
	while (cmd)
	{
		tmp = cmd;
		while (cmd)
		{
			tmp2 = cmd;
			free(cmd->argv);
			cmd = cmd->piped;
			free(tmp2);
		}
		cmd = tmp->new;
		free(tmp);
	}
}

int main(int argc, char **argv, char **envp)
{
	if (argc < 2)
		return (0);
	argv++;
	t_cmds *cmds;
	cmds = create_cmds(argv);
	exec_new(cmds, envp);
	free(cmds);

	return (0);
}