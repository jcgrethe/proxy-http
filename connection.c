int connectToServer(){
	// Close unused ends of pipes
    close(stdinFD);
    close(stdoutFD);

    stdinFD = stdoutFD = -1;

    close(pipeToChild[1]);
    close(pipeFromChild[0]);

    pipeToChild[1] = pipeFromChild[0] = -1;

    // Duplicate ends of pipes to stdin and stdout
    // Connect child stdin and stdout to parent pipes
    dup2(pipeToChild[0], fileno(stdin));
    dup2(pipeFromChild[1], fileno(stdout));

    // Build arguments for args. Note that bash is set as environment
    char * args[4];
    args[0] = "bash";
    args[1] = "-c";
    args[2] = command;
    args[3] = NULL;

    execvp(args[0], args);
}