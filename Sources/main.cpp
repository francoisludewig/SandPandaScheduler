#include "../Includes/Configuration/Option.h"
#include "../Includes/Model/SimulationScheduler.h"

using namespace std;

int main(const int argc,char **argv){
	Option opt;
	SimulationScheduler simulation_scheduler;
	if(opt.Management(argv,argc) == 0)
		return 0;

	// Vider la file d'attente avant d'arreter les simulations en cours : sinon un "-schedule"
	// (ou le prochain passage du cron) relancerait aussitot les simulations en attente.
	if (opt.flushPending) {
		simulation_scheduler.flushPending();
	}

	if (opt.stopRunning) {
		simulation_scheduler.stopRunning();
	}

	if (opt.add) {
		simulation_scheduler.add(opt.sand_panda_args(), opt.id1(), opt.threads_number1());
	}

	if (opt.schedule) {
		simulation_scheduler.scheduleSimulations();
	}

	// Apres -stopRunning : les simulations arretees sont passees en "completed" et sont nettoyees ici
	if (opt.clean) {
		simulation_scheduler.clean();
	}

	if (opt.resetPriority) {
		simulation_scheduler.resetPriority();
	}

	if (opt.change_numberThreads) {
		simulation_scheduler.update_max_number_threads(opt.number_threads());
	}

	// En dernier : affiche l'etat apres les autres actions de la commande
	if (opt.status) {
		simulation_scheduler.status();
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
