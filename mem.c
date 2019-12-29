#include "mem.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

//constante définie dans gcc seulement
// #ifdef __BIGGEST_ALIGNMENT__
// #define ALIGNMENT __BIGGEST_ALIGNMENT__
// #else
#define ALIGNMENT 8 //8 bits dans le cadre de ce tp
// #endif

/* La seule variable globale autorisée
 * On trouve à cette adresse la taille de la zone mémoire
 */
static void* memory_addr;

static inline void *get_system_memory_adr() {
	return memory_addr;
}

static inline size_t get_system_memory_size() {
	return *(size_t*)memory_addr;
}


struct fb {
	size_t size;
	struct fb* next;
};

struct gvb { //global variable bloc
	size_t tailleGlobale; //taille globale de la mémoire
	struct fb* list; //pointeur vers la première zone libre
	/* ... */ //il restera une autre variable à écrire ici (d'après le prof)
};


void mem_init(void* mem, size_t taille)
{
  memory_addr = mem; //initialise une zone libre en un seul bloc (mem)
  *(size_t*)memory_addr = taille; //initialise la taille du bloc

	assert(mem == get_system_memory_adr());
	assert(taille == get_system_memory_size());

	struct fb *ptr_fb = (struct fb*) (memory_addr + sizeof(struct gvb)); //on transforme le type void* en struct fb*
	ptr_fb->size = taille - sizeof(struct gvb);
	ptr_fb->next = NULL;

	struct gvb *ptr_gvb = (struct gvb*) memory_addr; //on transforme le type *void en struct gvb
	//un pointeur sur notre zone de variables global est créee.

	ptr_gvb->tailleGlobale = taille;
	ptr_gvb->list = ptr_fb;

	mem_fit(&mem_fit_first);// Politique appliqué dans le code : mem fit_first
}


void mem_show(void (*print)(void *, size_t, int)) {

	struct gvb *gvb = (struct gvb*) memory_addr;
	//un pointeur sur notre zone de variables global est créee.

	size_t tailleGlobale = gvb->tailleGlobale; //récupération taille globale zone mémoire
	struct fb *prochaine_ZL = gvb->list; // récupération première zone libre

	//récupérer l'@ du PREMIER bloc (libre ou non)
	void* addr = memory_addr + sizeof(struct gvb);
	//récupération de la taille du premier bloc courant
	size_t currentSizeBloc = ((struct fb*) (addr))->size; //cast struct fb mais pas forcément zone libre : ne surtout pas faire ->next !

	int compteur = sizeof(struct gvb);//itérateur sur le début de chaque zone (Libre ou non)

	printf("Ne faites attention que au MODE qui vous intéresse \n");
	printf("Mode Utilisateur (user) | Mode Développeur/Debug (memory)\n");
	printf("---\n");
	//tant qu'on a pas itérer sur chacunes des zones (libre ou non, on continue de boucler)
	while (compteur < tailleGlobale){

		if (prochaine_ZL == NULL || (struct fb*)addr < prochaine_ZL){
			//zone occupé
			printf("user   : ");
			print(addr,currentSizeBloc-8,0);
			printf("memory : ");
			print(addr,currentSizeBloc,0); printf("---\n");
		} else {
			//zone libre
			printf("user   : ");
			print(addr,currentSizeBloc-8,1);
			printf("memory : ");
			print(addr,currentSizeBloc,1); printf("---\n");

			prochaine_ZL = prochaine_ZL->next;//met à jour la prochaine zone lire;
		}
		compteur += currentSizeBloc; //met à jour l'itérateur
		addr = memory_addr + compteur; //met à jour la zone mémoire courante du bloc
		currentSizeBloc = ((struct fb*) (addr))->size; //met à jour la taille du bloc courant
	}
}

static mem_fit_function_t *mem_fit_fn;
void mem_fit(mem_fit_function_t *f) {
	mem_fit_fn=f;
}


/*
* On place correctement l'alignement dans la mémoire car on accède plus
* rapidement aux multiples de base de 2 (ici : ALIGNMENT = 8)
*/
size_t align(size_t taille){

	if ((taille % ALIGNMENT) == 0) {
		//si la taille est un multiple de 8, pas besoin de modifié
		return taille;

	} else {//sinon
		size_t quotient = taille/ALIGNMENT;
		return taille = ALIGNMENT*(quotient+1);
	}
}

/*
* Si la memoire restante après une allocation était inférieur ou égale à la taille d'une
* structure de bloc,
* alors il n'y a pas la place pour une Zone Libre, on considère donc cette espace comme occupé
*/
size_t alignZoneLibre(size_t tailleAOccuper, size_t tailleBloc){
	if (tailleBloc - tailleAOccuper <= sizeof(struct fb)){
		return tailleBloc;
	} else { //la taille A Occuper est déjà égale à la taille du Bloc
		return tailleAOccuper;
	}
}

void *mem_alloc(size_t tailleUtilisateur) {

	struct gvb *gvb = memory_addr; //on transforme le type *void en struct gvb
	__attribute__((unused)) /* juste pour que gcc compile ce squelette avec -Werror */

	size_t taille_zone_alloue = tailleUtilisateur + sizeof(size_t); //Remarque Prof (Séance 12/12/2019) : +align() à code
	taille_zone_alloue = align(taille_zone_alloue);

	struct fb *ptr_debut_zone_alloue = mem_fit_fn(gvb->list, taille_zone_alloue ); //on récupère la premiere zone libre possible en fonction de la taille

	//si il existe aucune zone que l'on peut allouer alors ..
	if (ptr_debut_zone_alloue == NULL) {
	 return NULL;
	}	else {

		//On veut vérifier qu'on a encore la place de créer une nouvelle zone libre après l'allocation de la mémoire.
		if( (ptr_debut_zone_alloue->size - taille_zone_alloue) > sizeof(struct fb) ){
			//cas où on peut allouer + créer une nouvelle zone libre à la suite

			/*

			AVANT :
				taille_zone_alloue (Demande de l'utilisateur + taille pour inscrire la taille du bloc)
							<---->
				gvb		|	Zone Libre | Zone Occupé | 		ZL   |   ZO 	|		ZL		|
				------|------------|-------------|---------|--------|---------|
							| 					 |						 |				 |				|					|
							|X					 |						 |				 |				|					|
						↓	 ↑ ↓												↑ ↓								 ↑ ↓
						↓→→↑ ↓→→→→→→→→→→→→→→→→→→→→→→→→↑ ↓→→→→→→→→→→→→→→→→↑ ↓→→→→→→→→→→→ NULL

							<------------>
				ptr_debut_zone_alloue->size

				L'idée c'est d'ajouter la zone occupé à a fin de la zone libre, afin de ne pas modifier X
				Nous avons choisis cette implémentation car elle nous paraissait plus simple à réaliser.
				Nota: l'origine de l'idée nous a été proposé par Théo Teyssier.

				APRÈS :

				gvb		|	ZL 	|  ZO	 | Zone Occupé | 		ZL   |   ZO 	|		ZL		|
				------|-----|------|-------------|---------|--------|---------|
							| 		|Z		 |						 |				 |				|					|
							|X		|A		 |						 |				 |				|					|
						↓	 ↑ ↓							       		↑ ↓								 ↑ ↓
						↓→→↑ ↓→→→→→→→→→→→→→→→→→→→→→→→→↑ ↓→→→→→→→→→→→→→→→→↑ ↓→→→→→→→→→→→ NULL

										<------>
				ptr_debut_zone_alloue->size

				X : ptr_debut_zone_alloue
				A : taille de la zone occupé, nommé taille_zone_alloue dans le code

			*/

			ptr_debut_zone_alloue->size = ptr_debut_zone_alloue->size - taille_zone_alloue; //Mise à jour de la taille de la nouvelle ZONE LIBRE
			*(size_t *) ((void*) ptr_debut_zone_alloue + ptr_debut_zone_alloue->size) = taille_zone_alloue; //Inscription Taille bloc zone alloué (A sur le schéma)

			return (void *)ptr_debut_zone_alloue + ptr_debut_zone_alloue->size + sizeof(size_t); //adresse vers la zone allouée (Z sur le schéma)

		} else if ( (ptr_debut_zone_alloue->size - taille_zone_alloue >= 0) &&
								(ptr_debut_zone_alloue->size - taille_zone_alloue <= sizeof(struct fb)) ){  // si il n'y a pas assez de place pour placer un bloc libre, on considère la zone comme occupé

			//cas où on alloue toute la zone libre

			/*

			AVANT :
taille_zone_alloue:	  cas 1ere ZL						 	cas Autre ZL
										<------------>						 <--------->
							gvb		|	Zone Libre | Zone Occupé | 		ZL   |   ZO 	|		ZL		|
							------|------------|-------------|---------|--------|---------|
										| 					 |						 |				 |				|					|
										|X					 |						 |Y				 |				|					|
									↓	 ↑ ↓												↑ ↓								 ↑ ↓
									↓→→↑ ↓→→→→→→→→→→→→→→→→→→→→→→→→↑ ↓→→→→→→→→→→→→→→→→↑ ↓→→→→→→→→→→→ NULL
			*/

			//cas de la toute première zone libre : (modification du gvb)
			if (ptr_debut_zone_alloue == gvb->list) {

				/*
				L'idée de mettre à jour le global variable bloc,
				notamment son premier élément (list) qui sera égale à la seconde zone LIBRE disponible

				APRÈS :

							gvb		|			ZO 		 | Zone Occupé | 		ZL   |   ZO 	|		ZL		|
							------|------------|-------------|---------|--------|---------|
										|X 		 			 |						 |				 |				|					|
										|A		   		 |						 |				 |				|					|
									↓	           									↑ ↓								 ↑ ↓
									↓→→→→→→→→→→→→→→→→→→→→→→→→→→→→→↑ ↓→→→→→→→→→→→→→→→→↑ ↓→→→→→→→→→→→ NULL

							X : ptr_debut_zone_alloue
							A : taille de la zone occupé, nommé taille_zone_alloue dans le code

				*/
				gvb->list = ptr_debut_zone_alloue->next;

				//Remarque Prof (Séance 12/12/2019) : Attention au fait de pas pouvoir avoir de ZONE LIBRE
				//si l'espace restant ne peut pas contenir plus de la taille d'une structure de bloc
				taille_zone_alloue = alignZoneLibre(taille_zone_alloue,ptr_debut_zone_alloue->size);
				*(size_t *) ((void*) ptr_debut_zone_alloue) = taille_zone_alloue; //Inscription Taille bloc zone alloué (A sur le schéma)

				return (void *)ptr_debut_zone_alloue + sizeof(size_t); // adresse vers la zone allouée (X sur le schéma)

			} else {
				//cas d'une quelconque autre zone libre

			  /*

				Idée : il faut parcourir toutes les zones libres jusqu'à atteindre celle dont on veut supprimer
				Ainsi, on modifie le "->next" de la zone libre précédente à celle supprimé

				APRÈS :

							gvb		|	Zone Libre | Zone Occupé | 		ZO   |   ZO 	|		ZL		|
							------|------------|-------------|---------|--------|---------|
										| 					 |						 |Y				 |				|					|
										| 					 |						 |A				 |				|					|
									↓	 ↑ ↓												  								 ↑ ↓
									↓→→↑ ↓→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→↑ ↓→→→→→→→→→→→ NULL

							Y : ptr_debut_zone_alloue
							A : taille de la zone occupé, nommé taille_zone_alloue dans le code

				*/

				//parcours de la liste des zones libres
				struct fb* current_zone_libre = gvb->list; //zone libre courante; (gvb->list n'est pas égal à NULL sinon on serait pas dans ce cas ci)
				int boolOnATrouveLePtrQuiPrecedeLaZoneAlloue = 0;

				//on boucle jusqu'à que current_zone_libre soit la zone libre juste avant celle à supprimer
				while (current_zone_libre != NULL || boolOnATrouveLePtrQuiPrecedeLaZoneAlloue == 0){

					if (current_zone_libre->next == ptr_debut_zone_alloue){ //si le ptr.next est égal à la zone libre qui va être modifié, alors on s'arrête
						boolOnATrouveLePtrQuiPrecedeLaZoneAlloue = 1;
					} else {
						current_zone_libre = current_zone_libre->next; //Attention à ne pas faire si on vient de changer le booléen !
					}
				}
				//mettre à jour current_zone_libre (il s'agit du pointeur qui précède)
				if (current_zone_libre->next != NULL){
					current_zone_libre = ptr_debut_zone_alloue->next;
				}

				taille_zone_alloue = alignZoneLibre(taille_zone_alloue,ptr_debut_zone_alloue->size); //si pas assez d'espace libre pour une zone libre supplémentaire
				*(size_t *) ((void*) ptr_debut_zone_alloue) = taille_zone_alloue; //Inscription taille bloc zone alloué (A sur le schéma)

				return (void *)ptr_debut_zone_alloue + sizeof(size_t); // adresse vers la zone allouée (Y sur le schéma)
			}

		} else { // ptr_debut_zone_alloue->size - taille < 0 : cas impossible en théorie; on a rajouter le ELSE car on passait pas la compilation
			return NULL;
		}
	}
}

/*
* Libération de la mémoire
*/
void mem_free(void* mem) {

	if ((size_t)mem % ALIGNMENT !=0){
		printf("Vous ne pouvez pas libérer à cette adresse, votre adresse n'est pas un multiple de 8 \n");
		printf("il est impossible qu'un bloc soit situé à cette adresse.\n");
	} else {
		//Pour libérer la mémoire, plusieurs cas possibles :
		//1er cas     - on libère une zone libre qui est situé avant la première zone libre actuellement connu (si il y en a une)
		// 		Dans cas on : change l'élément next du gvb, le nouveau bloc libéré a pour champ "next" la prochaine zone libre (null si non existant)
		//2eme cas   - on libère un bloc, et la zone qui le précède est égalment une zone libre.
		// 		Dans ce cas on : change juste la taille de la zone libre qui le précède (cela évite de créer une struct fb en plus en mémoire)
		//Autres cas - on libère le bloc occupé. On lui associe un struct fb en le chainant à une zone libre suivante.
		// ----
		//A chaque création de bloc, si celui ci est suivi d'une zone libre, on fusionnes les blocs

		struct gvb *gvb = memory_addr;
		struct fb* current_zone_libre = gvb->list; //zone libre courante;

		struct fb* zone_libre_a_inserer = NULL;
		size_t	taille_zone_a_liberer = *(size_t*)mem; //récupération de la taille de la mémoire à libérer

		//1er cas (si la zone à libérer est avant la première zone libre OU si toute la mémoire est déjà occupé)
		if (mem < (void*)current_zone_libre || current_zone_libre == NULL){

				zone_libre_a_inserer->size = taille_zone_a_liberer;
				zone_libre_a_inserer->next = current_zone_libre; //la zone qui vient d'être libéré est "chaîné" aux zones libres
				gvb->list = zone_libre_a_inserer; //on met à jour la tête de liste des zones libres car il s'agit de la première

			//pour réaliser les autres cas il faut connaitre la "nature" du bloc précédent
		} else {
			//on est sur que : il existe au moins une zone libre avant la zone à libérer.

			//détermination la nature du bloc précédent la zone à allouer:
			void* current_block = (void*)memory_addr + sizeof(struct gvb); //récupération du premier bloc
			struct fb* pred_zone_libre = NULL; //zone libre prédécent la zone à libérer.
			int isZonePrecedenteLibre = 0; //false

			while (current_block != NULL && current_block + sizeof(size_t) < mem){

				if (current_block == current_zone_libre) { // est-ce un bloc libre ?
					isZonePrecedenteLibre = 1; //true
					pred_zone_libre = current_zone_libre; //on garde en mémoire la zone libre précédente
					current_zone_libre = current_zone_libre->next;
				} else {
					isZonePrecedenteLibre = 0; //false
				}
				// Il faut mettre à jour la taille du block courant
				size_t tailleCouranteBloc = *(size_t*)current_block;
				current_block = current_block + tailleCouranteBloc;
			}

			printf("la zone prédécente est vide : %d\n",isZonePrecedenteLibre); //erreur quand la zone précédente est pas vide...

			//2ème cas (si la zone que l'on libère est précédé d'une zone libre alors on change uniquement la taille de celle ci)
			if (isZonePrecedenteLibre) {
				pred_zone_libre->size = pred_zone_libre->size + taille_zone_a_liberer;
			} else {
				//autres cas : on libère le bloc et on le "chaîne" aux autres zones libres
				zone_libre_a_inserer->size = taille_zone_a_liberer;
				zone_libre_a_inserer->next = current_zone_libre;
				pred_zone_libre->next = zone_libre_a_inserer;
			}
		} //fin traitements de chacun des cas

		//Erreurs :

		//Update : 28 décembre : 11h23
		//Le seul cas où le free fonctionne est celui ou on libère une zone et qu'il existe une zone libre précédente existante
		//Nous avons pas réussi à déterminer l'erreur.
		//Nous décidons de rendre le projet à ce jour, car nous sommes déjà en retard dans le rendu. (il me semble)


		//A chaque création de bloc, si celui ci est suivi d'une zone libre, on fusionnes les blocs
		/* à coder*/

		//NYI - Non codé pour le moment car intestable, en effet nous arrivons pas à libérer une zone lorsque qu'il y a une qui le précède
		//au vu de notre implémentation il est impossible d'avoir une zone libre après une zone occupé sans utiliser au moins le free (czr on occupe la mémoire en sens inverse du bloc)

	}
} //fin mem_free


struct fb* mem_fit_first(struct fb *list, size_t size) {

		//tant que la taille du bloc courant est inférieur à la taille demandé d'allocation, on passe au bloc libre suivant
		while(list != NULL && list->size < size){
			list = list->next;
		}
		return list;

}

/* Fonction à faire dans un second temps
 * - utilisée par realloc() dans malloc_stub.c
 * - nécessaire pour remplacer l'allocateur de la libc
 * - donc nécessaire pour 'make test_ls'
 * Lire malloc_stub.c pour comprendre son utilisation
 * (ou en discuter avec l'enseignant)
 */
size_t mem_get_size(void *zone) {
	/* zone est une adresse qui a été retournée par mem_alloc() */

	/* la valeur retournée doit être la taille maximale que
	 * l'utilisateur peut utiliser dans cette zone */
	return 0;
}

/* Fonctions facultatives
 * autres stratégies d'allocation
 */
struct fb* mem_fit_best(struct fb *list, size_t size) {
	return NULL;
}

struct fb* mem_fit_worst(struct fb *list, size_t size) {
	return NULL;
}
