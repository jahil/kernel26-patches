#include <stdio.h>

void do_child(int no) {
	sleep(20);
}

int main(int argc, char* argv[]) {
	
	int noforks=0;
	int pid = 0;
	int child = 0, childno = 0;

	noforks = atoi(argv[1]);

	while(noforks--) {
		pid=fork();
		if(pid == 0) {
			//child
			child = 1;
			childno = noforks;
			break;
		}
		//else just keep going
	}

	if(child) {
		do_child(noforks);
	}

	return 0;
}
