#ifndef __PROGTEST__
#include "common.h"
using namespace std;
#endif /* __PROGTEST__ */



struct CellOfCYK
{	
	CellOfCYK(): m_used (false), m_product(0), m_totalEnergy (0), m_L (NULL), m_R (NULL) {}
	CellOfCYK( bool used, uint8_t product, unsigned int energy, CellOfCYK * l, CellOfCYK * r)
         : m_used (used), m_product ( product ),  m_totalEnergy ( energy ), m_L ( l ), m_R ( r ) {}

	bool m_used;
	uint8_t m_product;
	unsigned int m_totalEnergy;
	CellOfCYK * m_L;
	CellOfCYK * m_R;
		
};


struct Task
{
	Task(): m_Request(NULL), m_Setup(NULL), m_ActualGenerator(0), m_FinishedGenerators(0) { }

	//zadani problemu
	const TRequest * m_Request;

	//ukladam dosavadni nejlepsi vysledek 
	TSetup * m_Setup;

	//aktualni generator, ktery se bude zpracovavat
	int m_ActualGenerator;

	//pocet jiz zpracovanych generatoru
	int m_FinishedGenerators;

};


//stack, mutex a 2 semafory, fce engines, generatory, pocet generatoru
typedef struct ThreadArg
{
  

  //const TRequest * stack[25]; /* zasobnik palivovych kruhu ke zpracovani */
  Task * stack[25];
  int actualPosition; /* aktualni pozice na zasobniku - prvni volna pozice na zasobniku 
  => vkladam na actualPosition(+1) a beru z pozice actualPosition-1 (-1) */
  pthread_mutex_t g_mut;       /* zamek pro kritickou sekci pristupu ke stacku */
  sem_t           g_cons;		/* semafor konzumenta */	
  sem_t           g_prod;  	/* semafor producenta */
  void           (* engines ) ( const TRequest * request, TSetup * setup);
  const TGenerator* m_generators;
  int               m_generatorsNr;

} THREADARG;



CellOfCYK * copyT(CellOfCYK * tmpNode)
{
	CellOfCYK * pomNode;


	if((tmpNode->m_L == NULL) & (tmpNode->m_R == NULL))
	{
		//cout << "nemam potomky" << endl;
		pomNode = new CellOfCYK(tmpNode->m_used, tmpNode->m_product, tmpNode->m_totalEnergy, NULL, NULL);
	}
	else
	{
		pomNode = new CellOfCYK(tmpNode->m_used, tmpNode->m_product, tmpNode->m_totalEnergy, copyT(tmpNode->m_L), copyT(tmpNode->m_R));
		
	}
	
	return pomNode;
}


CReactNode * copyTree(CellOfCYK *tmpNode)
{

	CReactNode * pomNode;
	

	if((tmpNode->m_L == NULL) & (tmpNode->m_R == NULL))
	{
		//cout << "nemam potomky" << endl;
		pomNode = new CReactNode(tmpNode->m_product, tmpNode->m_totalEnergy, NULL, NULL);
	}
	else
	{
		pomNode = new CReactNode(tmpNode->m_product, tmpNode->m_totalEnergy, copyTree(tmpNode->m_L), copyTree(tmpNode->m_R));
	}
	return pomNode;
}       


 void               optimizeEnergySeq                       ( const TGenerator* generators,
                                                             int               generatorsNr,
                                                             const TRequest  * request,
                                                             TSetup          * setup )
 {
	
	CellOfCYK TMP_rootMax;
 	unsigned int TMP_maxEnergy = 0;
 	int TMP_maxGenerator, TMP_startPos;   



   //prochazim dostupne generatory
 	for (int generatorCount = 0; generatorCount < generatorsNr; generatorCount++)
 	{
	 	//inicializuji pole
	   	CellOfCYK *** array = new CellOfCYK**[request->m_FuelNr];
		
		for(int i = 0; i < request->m_FuelNr; ++i)
		{
	    	array[i] = new CellOfCYK*[request->m_FuelNr];
	    	for (int j = 0; j < request->m_FuelNr; ++j)
	    	{
	    		array[i][j] = new CellOfCYK[6];
	    	}
	    }


	    //do spodni radky CYKu ulozim palivovy kruh, jakozto retezce o 1 znaku - nastavim prislusnym polickum true 
				//jakoze obsahuji tento prvek

		    	for (int i = 0; i < request->m_FuelNr; ++i)
		    	{
		    		array[0][i][request->m_Fuel[i]].m_used = true;
		    		array[0][i][request->m_Fuel[i]].m_product = request->m_Fuel[i];
		    	}

			    // postupne pro aktualni generator vyplnuju dalsi radky CYKU, tedy od radku 1 (na radku 0 jsou retezce o delce 1)
			    
			    for (int i = 1; i < request->m_FuelNr; ++i) //radky (od 1)
		    	{
		    		for (int j = 0; j < request->m_FuelNr; ++j) //sloupce (od 0)
		    		{
		    			for (int o = 0; o < i; ++o) //CYK
		    			{
		    				for (int p = 0; p < 6; ++p)  //kvarky reagujiciho 1.policka
		    				{
		    					//pokud neni kv. 1.policka continue;
		    					if (array[o][j][p].m_used == false)
		    					{
		    						continue;
		    					}

		    					for (int q = 0; q < 6; ++q)   //kvarky reagujiciho 2.policka
		    					{	
		    						//pokud neni kv. 2.policka continue;
		    						if (array[(i-o-1)][((j+o+1)%request->m_FuelNr)][q].m_used == false)
			    					{
			    						continue;
			    					}

		    						for (int r = 0; r < 6; ++r)    //vysledny kvark reakce
		    						{
		    							if ( (array[o][j][p].m_used == true) 
		    							&& (array[(i-o-1)][((j+o+1)%request->m_FuelNr)][q].m_used == true)
		    							&& (generators[generatorCount].m_Energy[p][q][r] > 0))
		    							{
		    								if (array[o][j][p].m_totalEnergy + array[(i-o-1)][((j+o+1)%request->m_FuelNr)][q].m_totalEnergy + generators[generatorCount].m_Energy[p][q][r] > array[i][j][r].m_totalEnergy)
		    								{
			    								//pridavam do CYKu option 
			    								array[i][j][r].m_used = true;
			    								array[i][j][r].m_product = r;
			    								//scitam doposud ziskane energie reagujicich kvarku a pricitam energii ziskanou touto reakci
			    								array[i][j][r].m_totalEnergy = array[o][j][p].m_totalEnergy + array[(i-o-1)][((j+o+1)%request->m_FuelNr)][q].m_totalEnergy + generators[generatorCount].m_Energy[p][q][r];
			    								//prirazuju odkaz na reagujici prvky jako podstromy tohoto vznikleho prvku
			    								array[i][j][r].m_L = &array[o][j][p];
			    								array[i][j][r].m_R = &array[(i-o-1)][((j+o+1)%request->m_FuelNr)][q];
		    								}
	    								}
		    						}
		    					}
		    				}
		    			}
		    		}
		    	}
	   
		//vyhodnotim, zda je tento vysledek lepsi nez vysledek predchoziho generatoru - nechavam si pouze prvni nejlepsi vysledek
		    	//prochazim pouze nejvyssi radek CYKU
		    	for (int i = 0; i < request->m_FuelNr; ++i)
		    	{
		    		//prochazim pouze energie u vzniklych nami pozadovanych kvarku
		    		if (array[request->m_FuelNr-1][i][request->m_FinalProduct].m_totalEnergy > TMP_maxEnergy) // segfault
		    		{
		    			//mam novou nejvyssi uvolnenou energii s vzniklym pozadovanym kvarkem
		    			TMP_maxEnergy = array[request->m_FuelNr-1][i][request->m_FinalProduct].m_totalEnergy;
		    			TMP_startPos = i;
		    			TMP_maxGenerator = generatorCount;
	
		    			TMP_rootMax.m_L = NULL;
		    			TMP_rootMax.m_R = NULL;
						
						TMP_rootMax.m_used = true;
		    			TMP_rootMax.m_product = array[request->m_FuelNr-1][i][request->m_FinalProduct].m_product;
		    			TMP_rootMax.m_totalEnergy = array[request->m_FuelNr-1][i][request->m_FinalProduct].m_totalEnergy;
		    			TMP_rootMax.m_L = copyT(array[request->m_FuelNr-1][i][request->m_FinalProduct].m_L);
		    			TMP_rootMax.m_R = copyT(array[request->m_FuelNr-1][i][request->m_FinalProduct].m_R);

		    			//kdyby byl vysledny nejaky dalsi generator, premazu jim tenhle
		    		}
		    	}

		    	
		//destrukce pole
			  	for(int i = 0; i < request->m_FuelNr; ++i) 
			    {
				    for (int j = 0; j < request->m_FuelNr; ++j)
			        {
			                delete [] array[i][j];
			        }
			        delete [] array[i];
				}
				delete [] array;	
				
		}


	//sestavuju CReactNode a strom do setupu

	if(TMP_maxEnergy > 0)
	{
		setup->m_Energy = TMP_maxEnergy;
		setup->m_Generator = TMP_maxGenerator;
		setup->m_StartPos = TMP_startPos;
		setup->m_Root = copyTree(&TMP_rootMax);
	}
	else
	{
		setup->m_Root = NULL;
  		setup->m_Energy = 0;
	}


 }                                                         



//predavam sekvencnimu reseni: generatory, aktualni zpracovavanej generator, request, ukazatel na setup
void               optimizeEnergySeqForThread                ( const TGenerator* generators,
                                                             int               actualGenerator,
                                                             const TRequest * request, 
                                                             TSetup * setup )
 {

 		setup->m_Root = NULL;
 		setup->m_Energy = 0;
 		//setup->m_Generator = 0;
 		//setup->m_StartPos = 0;


	 	//inicializuji pole
	   	CellOfCYK *** array = new CellOfCYK**[request->m_FuelNr];
		
		for(int i = 0; i < request->m_FuelNr; ++i)
		{
	    	array[i] = new CellOfCYK*[request->m_FuelNr];
	    	for (int j = 0; j < request->m_FuelNr; ++j)
	    	{
	    		array[i][j] = new CellOfCYK[6];
	    	}
	    }


	    //do spodni radky CYKu ulozim palivovy kruh, jakozto retezce o 1 znaku - nastavim prislusnym polickum true 
				//jakoze obsahuji tento prvek

		    	for (int i = 0; i < request->m_FuelNr; ++i)
		    	{
		    		array[0][i][request->m_Fuel[i]].m_used = true;
		    		array[0][i][request->m_Fuel[i]].m_product = request->m_Fuel[i];
		    	}

			    // postupne pro aktualni generator vyplnuju dalsi radky CYKU, tedy od radku 1 (na radku 0 jsou retezce o delce 1)
			    
			    for (int i = 1; i < request->m_FuelNr; ++i) //radky (od 1)
		    	{
		    		for (int j = 0; j < request->m_FuelNr; ++j) //sloupce (od 0)
		    		{
		    			for (int o = 0; o < i; ++o) //CYK
		    			{
		    				for (int p = 0; p < 6; ++p)  //kvarky reagujiciho 1.policka
		    				{
		    					//pokud neni kv. 1.policka continue;
		    					if (array[o][j][p].m_used == false)
		    					{
		    						continue;
		    					}

		    					for (int q = 0; q < 6; ++q)   //kvarky reagujiciho 2.policka
		    					{	
		    						//pokud neni kv. 2.policka continue;
		    						if (array[(i-o-1)][((j+o+1)%request->m_FuelNr)][q].m_used == false)
			    					{
			    						continue;
			    					}

		    						for (int r = 0; r < 6; ++r)    //vysledny kvark reakce
		    						{
		    							if ( (array[o][j][p].m_used == true) 
		    							&& (array[(i-o-1)][((j+o+1)%request->m_FuelNr)][q].m_used == true)
		    							&& (generators[actualGenerator].m_Energy[p][q][r] > 0))
		    							{
		    								if (array[o][j][p].m_totalEnergy + array[(i-o-1)][((j+o+1)%request->m_FuelNr)][q].m_totalEnergy + generators[actualGenerator].m_Energy[p][q][r] > array[i][j][r].m_totalEnergy)
		    								{
			    								//pridavam do CYKu option 
			    								array[i][j][r].m_used = true;
			    								array[i][j][r].m_product = r;
			    								//scitam doposud ziskane energie reagujicich kvarku a pricitam energii ziskanou touto reakci
			    								array[i][j][r].m_totalEnergy = array[o][j][p].m_totalEnergy + array[(i-o-1)][((j+o+1)%request->m_FuelNr)][q].m_totalEnergy + generators[actualGenerator].m_Energy[p][q][r];
			    								//prirazuju odkaz na reagujici prvky jako podstromy tohoto vznikleho prvku
			    								array[i][j][r].m_L = &array[o][j][p];
			    								array[i][j][r].m_R = &array[(i-o-1)][((j+o+1)%request->m_FuelNr)][q];
		    								}
	    								}
		    						}
		    					}
		    				}
		    			}
		    		}
		    	}
	
		    	//prochazim pouze nejvyssi radek CYKU a hledam nejlepsi vysledek v aktualnim generatoru
		    	CellOfCYK TMP_rootMax;
		    	TMP_rootMax.m_totalEnergy = 0;
			 	int TMP_startPos; 

		    	for (int i = 0; i < request->m_FuelNr; ++i)
		    	{
		    		
		    		//prochazim pouze energie u vzniklych nami pozadovanych kvarku
		    		if (array[request->m_FuelNr-1][i][request->m_FinalProduct].m_totalEnergy > TMP_rootMax.m_totalEnergy) 
		    		{
		    			//mam novou nejvyssi uvolnenou energii s vzniklym pozadovanym kvarkem
		    			TMP_rootMax.m_totalEnergy = array[request->m_FuelNr-1][i][request->m_FinalProduct].m_totalEnergy;
		    			TMP_startPos = i;
						
		    			TMP_rootMax.m_product = array[request->m_FuelNr-1][i][request->m_FinalProduct].m_product;
		    			TMP_rootMax.m_L = copyT(array[request->m_FuelNr-1][i][request->m_FinalProduct].m_L);
		    			TMP_rootMax.m_R = copyT(array[request->m_FuelNr-1][i][request->m_FinalProduct].m_R);

		    		}
		    	}

		//destrukce pole
			  	for(int i = 0; i < request->m_FuelNr; ++i) 
			    {
				    for (int j = 0; j < request->m_FuelNr; ++j)
			        {
			                delete [] array[i][j];
			        }
			        delete [] array[i];
				}
				delete [] array;	
				
		


	//sestavuju CReactNode a strom do setupu

		if(TMP_rootMax.m_totalEnergy > 0)
		{
			setup->m_Energy = TMP_rootMax.m_totalEnergy;
			setup->m_Generator =  actualGenerator;
			setup->m_StartPos = TMP_startPos;
			setup->m_Root = copyTree(&TMP_rootMax);
		}
		else
		{
			setup->m_Root = NULL;
	  		setup->m_Energy = 0;
		}

 }


//konzument - funkce vlakna, tedy typu void *(*) (void *)
void * consument (void * parameters)
{
	
	//cout << "Jsem v konzumentovi." << endl;
	TSetup * tmpSetup = new TSetup(); 
	bool flag = false;

  	struct ThreadArg *p = (struct ThreadArg *) parameters;


  	while(1)
	{	

		//cout << "Jsem ve whilu v konzumentovi." << endl;

		
		Task * tsk;
		int generatorToDo;
		
		sem_wait(&p->g_cons);
		pthread_mutex_lock ( &p->g_mut);


			//cout << "jsem v prvnim mutexu" << " p->actualPosition: " << p->actualPosition << endl;
			//koukam se na stack, zda mam co na praci, kdyz ne, tak break; 
			if(p->actualPosition == 0)
			{
				pthread_mutex_unlock ( &p->g_mut );
				break;
			}
			

			//beru si aktualni task a zjistim, jaky generator mam provadet
			tsk = p->stack[(p->actualPosition-1)];
			generatorToDo = p->stack[(p->actualPosition-1)]->m_ActualGenerator;


			//pokud jiz zpracovavam tuto ulohu pro posledni generator
			if (tsk->m_ActualGenerator == (p->m_generatorsNr-1))
			{
				//odstranim ulohu ze stacku - aktualni pozici posouvam o -1 -> musim ji vyhodnotit, zavolat engines a smazat task
				p->stack[(p->actualPosition-1)] = NULL;
	     		p->actualPosition--;
	     		flag = true;
			} 

			tsk->m_ActualGenerator++;
		
		pthread_mutex_unlock ( &p->g_mut );

		if(flag == true)
		{
			sem_post(&p->g_prod);
			flag = false;
		}
		

		//volam seq reseni
		//predavam sekvencnimu reseni: generatory, aktualni zpracovavanej generator, request, ukazatel na setup
		//do lokalniho setupu si ulozim nejlepsi reseni daneho kruhu pro dany generator
		optimizeEnergySeqForThread(p->m_generators, generatorToDo, tsk->m_Request, tmpSetup);

		



		//vracim se ze sekvencniho reseni - musim vyhodnotit, zda je vysledek (setup) lepsi nez z predchozich generatoru
		pthread_mutex_lock ( &p->g_mut);


			//kouknu se zda moje reseni je lepsi nez to maximalni, pokud ano nahradim ho
			if (tmpSetup->m_Energy > tsk->m_Setup->m_Energy)
			{
				//ukladam tmpSetup jako aktualne nejlepsi vysledek
				tsk->m_Setup->m_Energy = tmpSetup->m_Energy;
				tsk->m_Setup->m_Generator = tmpSetup->m_Generator;
				tsk->m_Setup->m_StartPos = tmpSetup->m_StartPos;
				tsk->m_Setup->m_Root = tmpSetup->m_Root;
			}

			//k cislu jiz hotovych generatoru prictu 1
			tsk->m_FinishedGenerators++;



			//Pokud uz jsou vsechny generatory hotovy, pak odevzdam maximalni setup.
			if(tsk->m_FinishedGenerators == p->m_generatorsNr)
			{
				p->engines(tsk->m_Request, tsk->m_Setup);

				//A případně můžu task smazat z paměti, ale to není nutné.
				tsk->m_Request = NULL;
				tsk->m_Setup = NULL;
				tsk->m_ActualGenerator = 0;
				tsk->m_FinishedGenerators = 0;

			}

		pthread_mutex_unlock ( &p->g_mut );

	}

	delete tmpSetup;
	return (NULL);

	


	return (NULL);
}

void               optimizeEnergy                          ( int               threads,
                                                             const TGenerator* generators,
                                                             int               generatorsNr,
                                                             const TRequest *(* dispatcher)( void ),
                                                             void           (* engines ) ( const TRequest * request, TSetup * setup) )
 {


 	const TRequest * req;

 	//cout << "Jsem v optimizeEnergy. Pocet generatoru: " << generatorsNr << endl;

 	//atribut - pokud vlaknu predam jako atribut NULL budou nastaveny implicitni hodnoty, 
 	//ja ale chci aby bylo joinable...
   	pthread_attr_t   attr;
   	//identifikacni cisla vlaken, pole s vlakny
   	pthread_t * cons;
   	cons = new pthread_t[threads];

   	//instance struktury ve ktere budou informace pro vlakna - stack, mutex a 2 semafory, fce engines, generatory, pocet generatoru
   	ThreadArg * argForThread = new ThreadArg();
   	// inicializuju veci z globalni struktury
  	argForThread->actualPosition = 0; /* aktualni pozice na zasobniku - prvni volna pozice na zasobniku 
  	=> vkladam na actualPosition(+1) a beru z pozice actualPosition-1 (-1) */
  	argForThread->m_generatorsNr = generatorsNr;
  	argForThread->m_generators = generators;
  	argForThread->engines = engines;
 
   	//semafor a mutex - semafory konzument a producent...
   	pthread_mutex_init ( &argForThread->g_mut, NULL );
	//sem_init(ukazatel na promennou sem_t, 0 - linux nic jineho nepodporuje..., 0 - pocatecni hodnota semaforu);
   	sem_init(&argForThread->g_cons, 0, 0);
   	sem_init(&argForThread->g_prod, 0, 25);

   	/* Inicializuji atribut vlakna, nastavim schopnot "byt pouzit s pthread_join()" - atribut stav odlouceni */
   pthread_attr_init ( &attr );
   pthread_attr_setdetachstate ( &attr, PTHREAD_CREATE_JOINABLE );

   //pthread_create() - 4.parametr: Argument vlákna pro přenos dat typu void *. Pomocí tohoto argumentu můžete předávat funkci jakákoliv data.
   //ja potrebuju predavat instanci struktury argForThread
   	for ( int t = 0; t < threads; t++ )
   	{
    	pthread_create ( &cons[t], &attr, (void*(*)(void*)) consument, (void *) argForThread);
    } 
     
     //neni nutne uchovavat po vytvoreni vlaken
    pthread_attr_destroy ( &attr ); 
  
  //producent
     //dokud dispatcher neco vraci a zaroven mam kam to produkovat, tedy mam misto na stacku...
     while((req = dispatcher()) != NULL)
     {		
     			//cout << "Jsem ve whilu v producentovi." << endl;
     			
       			Task * tsk  = new Task();
       			//inicializuju...
 				TSetup * set = new TSetup();
 				set->m_Energy = 0;
 				
 				tsk->m_Setup = set;
				tsk->m_Request = req;
     			
     			//tsk->m_FinishedGenerators = 0;
     			//tsk->m_ActualGenerator = 0;

				sem_wait(&argForThread->g_prod);
     			pthread_mutex_lock ( &argForThread->g_mut);
     			//pridavam ulohu na stack - aktualni pozici posouvam o +1
     			argForThread->stack[argForThread->actualPosition] = tsk;
     			argForThread->actualPosition++; 
				
				pthread_mutex_unlock ( &argForThread->g_mut );
				for (int i = 0; i < generatorsNr; i++)
				{
					sem_post(&argForThread->g_cons);
				}
     			
				
     }
     
     for ( int d = 0; d < threads; d++)
     {
        sem_post(&argForThread->g_cons);
     }

   /* Ceka na ukonceni vsech ostatnich vlaken */
   for ( int d = 0; d < threads; d++ )
   {
  		pthread_join(cons[d], NULL);
   }


 	delete [] cons;
 	pthread_mutex_destroy ( &argForThread->g_mut );
 	sem_destroy( &argForThread->g_cons );
 	sem_destroy( &argForThread->g_prod );
 	delete argForThread;

 }
