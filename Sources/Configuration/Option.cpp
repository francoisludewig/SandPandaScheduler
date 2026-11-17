#include "../../Includes/Configuration/Option.h"

#include <cstring>
#include <iostream>

Option::Option() noexcept = default;

int Option::Management(char **argv, const int argc) noexcept {
	list = argv;
	Nlist = argc;
	for(int i = 1 ; i < argc ; i++){
		if(argv[i][0] == 45){
			switch(argv[i][1]){
				case 'h':
					if(!strcmp(argv[i], "-help")){
						printf("SandPandaSchedule\nOptions are :\n");
						printf("\t-add allow to add a pending simulation\n");
						printf("\t-schedule start analyse of running simulations and start new one if resources are available\n");
						printf("\t-clean remove terminated simulations and simulations in error from history\n");
						printf("\t-numberThreads allow to set the maximum number of thread managed by the scheduler\n");
						printf("\t-flushPending remove all pending simulations\n");
						printf("\t-stopRunning stop all running simulations (SIGTERM, then SIGKILL after 10 s)\n");
						printf("\t-resetPriority reset the priority counter to 1 (or just after the last pending simulation)\n");
						printf("\t-reset full reset : -stopRunning, -clean and -resetPriority (pending simulations are kept)\n");
						printf("\t-status print threads, running and pending simulations and the priority counter\n");
						return 0;
					}
					if(!strcmp(argv[i], "-h")){
						printf("SandPandaSchedule\nOptions are :\n");
						printf("\t-add allow to add a pending simulation\n");
						printf("\t-schedule start analyse of running simulations and start new one if resources are available\n");
						printf("\t-clean remove terminated simulations and simulations in error from history\n");
						printf("\t-numberThreads allow to set the maximum number of thread managed by the scheduler\n");
						printf("\t-flushPending remove all pending simulations\n");
						printf("\t-stopRunning stop all running simulations (SIGTERM, then SIGKILL after 10 s)\n");
						printf("\t-resetPriority reset the priority counter to 1 (or just after the last pending simulation)\n");
						printf("\t-reset full reset : -stopRunning, -clean and -resetPriority (pending simulations are kept)\n");
						printf("\t-status print threads, running and pending simulations and the priority counter\n");
						return 0;
					}
					break;
				case 'a':
					if(!strcmp(argv[i], "-add")){
						printf("add\n");
						// Get SandPanda arguments
						sandPandaArgs = std::string(argv[i+1]);
						i++;
						printf("arg = %s\n", sandPandaArgs.c_str());
						// Get id
						id = std::string(argv[i+1]);
						i++;
						printf("id = %s\n", id.c_str());

						sscanf(argv[i+1], "%d", &threads_number);
						printf("NThreads = %d\n", threads_number);
						i++;
						add = true;
					}
					break;
				case 's':
					if(!strcmp(argv[i], "-schedule")){
						printf("schedule\n");
						schedule = true;
					}
					if(!strcmp(argv[i], "-status")){
						status = true;
					}
					if(!strcmp(argv[i], "-stopRunning")){
						printf("stop running\n");
						stopRunning = true;
					}
					break;
				case 'f':
					if(!strcmp(argv[i], "-flushPending")){
						printf("flush pending\n");
						flushPending = true;
					}
					break;
				case 'r':
					if(!strcmp(argv[i], "-resetPriority")){
						printf("reset priority\n");
						resetPriority = true;
					}
					if(!strcmp(argv[i], "-reset")){
						printf("reset : stop running, clean, reset priority\n");
						stopRunning = true;
						clean = true;
						resetPriority = true;
					}
					break;
				case 'c':
					if(!strcmp(argv[i], "-clean")){
						printf("clean\n");
						clean = true;
					}
					break;
					case 'n':
					if(!strcmp(argv[i], "-numberThreads")) {
						sscanf(argv[i+1], "%d", &numberThreads);
						printf("numberThreads = %d\n", numberThreads);
						i++;
						change_numberThreads = true;
					}
				default: ;
			}
		}
	}
	return 1;
}