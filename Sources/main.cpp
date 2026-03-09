#include "../Includes/Configuration/Option.h"
#include "../Includes/Model/SimulationScheduler.h"

using namespace std;

int main(const int argc,char **argv){
	Option opt;
	SimulationScheduler simulation_scheduler;
	if(opt.Management(argv,argc) == 0)
		return 0;

	if (opt.add) {
		simulation_scheduler.add(opt.sand_panda_args(), opt.id1(), opt.threads_number1());
	}

	if (opt.schedule) {
		simulation_scheduler.scheduleSimulations();
	}

	if (opt.clean) {
		simulation_scheduler.clean();
	}

	if (opt.change_numberThreads) {
		simulation_scheduler.update_max_number_threads(opt.number_threads());
	}

	/* TODO
	 * (i) Le programme reçoit le script à lancer ou le créé sur base des arguments de simulations + prévoir la variable d'environnement pour le nombre de thread OpenMP
	 * (ii) Proposer une option pour lancer le post traitement après la simulation (dans le script si généré par le programme)
	 * (iii) Schedule (a) Vérifier les Running (update des statuts) (b) lancer une Pending si une running est Completed
	 * (iv) Fixer dans une configuration le nombre maximal de threads / process sur la machine
	 * (v) Installer les logiciels de simulation et d'analyse (bin) et utiliser CRON pour lancer ce programme toutes les heures par exemple
	 * /!\ Le add doit faire un schedule pour lancer immédiatement une simulation si resource disponible
	 * (vi) prévoir un système de priorité pour ajouter une simulation prioritaire (ou simplement en haut de la pile des pending)
	 */
	return 0;
}
