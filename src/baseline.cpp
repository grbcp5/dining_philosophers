#include <cstdlib>
#include <iostream>
#include <cerrno>
#include <unistd.h>
#include "mpi.h"

//run compiled code (for 5 philosophers) with mpirun -n 6 program

using namespace std;

/* Message protocol defines */
#define I_WANT_TO_EAT 1
#define IM_DONE_EATING 2
#define YOU_CAN_EAT 3
#define YOU_CAN_NOT_EAT 4

#define DEBUG false
#define MAX_SLEEP_TIME 10

#define MASTER_THREAD 0
#define PHILOSOPHER_THREAD( philos_id ) ( philos_id + 1 )

class Fork {
private:
	bool m_isDown;
	
public:

	/* Constructor */
	Fork() {
		this->m_isDown = true;
	}
	
	/* Call to change the forks state to "up" */
	bool pickUp() {
		if( this->m_isDown ) {
			this->m_isDown = false;
			return true;
		}
		
		/* Fork is already up */
		return false;
	}
	
	bool tryPickUp() {
		return ( this->m_isDown );
	}
	
	/* Call to change the forks state to "down" */
	bool setDown() {
		if( !( this->m_isDown ) ) {
			this->m_isDown = true;
			return true;
		}
		
		/* Fork is already down */
		return false;
	}
	
	bool trySetDown() {
		return !( this->m_isDown );
	}
	
	bool isDown() {
		return this->m_isDown;
	}
};
typedef Fork* ForkPtr;



class Philosopher {
private:
	bool m_isEating;

public:
	Philosopher() {
		this->m_isEating = false;
	}
	
	bool startEating() {
		
		if( !( this->m_isEating ) ) {
			this->m_isEating = true;
			return true;
		}
		
		/* Philosopher is already eating */
		return false;
	}
	
	bool tryStartEating() {
		return !( this->m_isEating );
	}
	
	bool stopEating() {
		
		if( this->m_isEating ) {
			this->m_isEating = false;
			return true;
		}
		
		/* Philosopher is not currently eating */
		return false;
	}
	
	bool tryStopEating() {
		return ( this->m_isEating );
	}

};
typedef Philosopher* PhilosopherPtr;

struct PlaceSetting {
	ForkPtr fork1;
	ForkPtr fork2;
};

class Table {
private:
	int m_numPhilos;
	ForkPtr* m_forks;
	PhilosopherPtr* m_philosophers;
	
public:
	
	/* Constructor */
	Table( const int numPhilosophers ) {
		this->m_numPhilos = numPhilosophers;
		
		/* Create fork array */
		this->m_forks = new ForkPtr[ numPhilosophers ];
		for( int f = 0; f < numPhilosophers; f++ ) {
			this->m_forks[ f ] = new Fork();
		}
		
		/* Create philosopher array */
		this->m_philosophers = new PhilosopherPtr[ numPhilosophers ];
		for( int p = 0; p < numPhilosophers; p++ ) {
			this->m_philosophers[ p ] = new Philosopher();
		}
		
	}

	int getNumPhilosophers() {
		return this->m_numPhilos;
	}
	
	ForkPtr getForkAt( const int forkIdx ) {
		if( !( 0 <= forkIdx && forkIdx < this->m_numPhilos ) ) {
			return NULL;
		}
		
		return this->m_forks[ forkIdx ];
	}
	
	PlaceSetting* getPlaceSettingForPhilosopherAt( const int philosIdx ) {
		if( !( 0 <= philosIdx && philosIdx < this->m_numPhilos ) ) {
			return NULL;
		}
		
		PlaceSetting* result = new PlaceSetting();
		
		result->fork1 = this->m_forks[ philosIdx ];
		result->fork2 = this->m_forks[ ( philosIdx + 1 ) % this->m_numPhilos ];
		
		return result;
	}
	
	
	PhilosopherPtr getPhilosopherAt( const int philosIdx ) {
		if( !( 0 <= philosIdx && philosIdx < this->m_numPhilos ) ) {
			return NULL;
		}
		
		return this->m_philosophers[ philosIdx ];
	}
	
};


int leftIdx( int idx, int numPhilosophers ) {
	if( idx == 0 ) {
		return numPhilosophers - 1;
	}
	return idx - 1;
}

int rightIdx( int idx, int numPhilosophers ) {
	return ( idx + 1 ) % numPhilosophers;
}

int main ( int argc, char *argv[] )
{
  int id; //my MPI ID
  int p;  //total MPI processes
  MPI::Status status;
  int tag = 1;

  //  Initialize MPI.
  MPI::Init ( argc, argv );

  //  Get the number of processes.
  p = MPI::COMM_WORLD.Get_size ( );

  //  Determine the rank of this process.
  id = MPI::COMM_WORLD.Get_rank ( );

  //Safety check - need at least 2 philosophers to make sense
  if (p < 3) {
	    MPI::Finalize ( );
	    std::cerr << "Need at least 2 philosophers! Try again" << std::endl;
	    return 1;
  }

  srand(id + time(NULL)); //ensure different seeds...

  //  Setup Fork Master (Ombudsman) and Philosophers
  if ( id == 0 ) //Master
  {
	int msgIn; //messages are integers
	int msgOut;
	int phil_id;
	int numPhilos( p - 1 );
	PlaceSetting* philTableSetting;
	Table table( numPhilos );
	bool* waiting;
	
	waiting = new bool[ numPhilos ];
	for( int p = 0; p < numPhilos; p++ ) {
		waiting[ p ] = false;
	}
	
	//let the philosophers check in
    while( true ) {
  		MPI::COMM_WORLD.Recv ( &msgIn, 1, MPI::INT, MPI::ANY_SOURCE, tag, status );
	
		switch( msgIn ) {
		case I_WANT_TO_EAT:
			phil_id = status.Get_source() - 1;
			
			std::cout << "Philosopher " << phil_id << " wants to eat." << std::endl;
			
			philTableSetting = table.getPlaceSettingForPhilosopherAt( phil_id );
			if( philTableSetting->fork1->isDown() ) {
				
				if( philTableSetting->fork2->isDown() ) {
					
					std::cout << "Philosopher " << phil_id << " can eat." << std::endl;
					
					/* Try to start eating */
					if( !( table.getPhilosopherAt( phil_id )->startEating() ) ||
					!( philTableSetting->fork1->pickUp() ) ||
					!( philTableSetting->fork2->pickUp() )  ) {
						std::cout << "Error in making philosopher " 
						<< phil_id 
						<< " start eating. " << std::endl;
						
						if( DEBUG ) {
							return 1;
						} else {
							continue;
						}
						
					}
					
					std::cout << "Signaling to philosopher " << phil_id << " to begin eating." << std::endl;
					msgOut = YOU_CAN_EAT;
					MPI::COMM_WORLD.Send( &msgOut, 1, MPI::INT, PHILOSOPHER_THREAD( phil_id ), tag );
					
				} else { // Fork 2 is not down
					
					std::cout << "Philosopher " << phil_id << " can't eat because fork "
					<< ( ( phil_id + 1 ) % numPhilos )
					<< " is not on the table" << std::endl;
					waiting[ phil_id ] = true;
				}
				
			} else { // Fork 1 is not down
				
				std::cout << "Philosopher " << phil_id << " can't eat because fork "
				<< ( phil_id )
				<< " is not on the table" << std::endl;
				waiting[ phil_id ] = true;
				
			}
			
			break;
		
		case IM_DONE_EATING:
			phil_id = status.Get_source() - 1;
			
			std::cout << "Philosopher " << phil_id << " says he's done eating." << std::endl;
			
			philTableSetting = table.getPlaceSettingForPhilosopherAt( phil_id );
			
			/* Try to stop eating */
			if( !( table.getPhilosopherAt( phil_id )->stopEating() ) ||
				!( philTableSetting->fork1->setDown() ) ||
				!( philTableSetting->fork2->setDown() )  ) {
					
					std::cout << "Error in making philosopher " 
					<< phil_id 
					<< " stop eating. " << std::endl;
						
					if( DEBUG ) {
						return 1;
					} else {
						continue;
					}
			}
			
			
			std::cout << "Philosopher " << phil_id << " is now not eating." << std::endl;
			
			if( waiting[ leftIdx( phil_id, numPhilos ) ] ) {
				
				/* Try to start eating */
				philTableSetting = table.getPlaceSettingForPhilosopherAt( leftIdx( phil_id, numPhilos ) );
				if( ( table.getPhilosopherAt( leftIdx( phil_id, numPhilos ) )->tryStartEating() ) &&
				( philTableSetting->fork1->tryPickUp() ) &&
				( philTableSetting->fork2->tryPickUp() )  ) {
					
					table.getPhilosopherAt( leftIdx( phil_id, numPhilos ) )->startEating();
					philTableSetting->fork1->pickUp();
					philTableSetting->fork2->pickUp();
					
					std::cout << "Philosopher " << leftIdx( phil_id, numPhilos ) << " can now eat." << std::endl;
					std::cout << "Signaling to philosopher " << leftIdx( phil_id, numPhilos ) << " to begin eating." << std::endl;
					msgOut = YOU_CAN_EAT;
					MPI::COMM_WORLD.Send( &msgOut, 1, MPI::INT, PHILOSOPHER_THREAD( leftIdx( phil_id, numPhilos ) ), tag );
					waiting[ leftIdx( phil_id, numPhilos ) ] = false;				
					
				}
				
			}
			
			if( waiting[ rightIdx( phil_id, numPhilos ) ] ) {
				
				/* Try to start eating */
				philTableSetting = table.getPlaceSettingForPhilosopherAt( rightIdx( phil_id, numPhilos ) );
				if( ( table.getPhilosopherAt( rightIdx( phil_id, numPhilos ) )->tryStartEating() ) &&
				( philTableSetting->fork1->tryPickUp() ) &&
				( philTableSetting->fork2->tryPickUp() )  ) {
					
					table.getPhilosopherAt( rightIdx( phil_id, numPhilos ) )->startEating();
					philTableSetting->fork1->pickUp();
					philTableSetting->fork2->pickUp();
					
					std::cout << "Philosopher " << rightIdx( phil_id, numPhilos ) << " can now eat." << std::endl;
					std::cout << "Signaling to philosopher " << rightIdx( phil_id, numPhilos ) << " to begin eating." << std::endl;
					msgOut = YOU_CAN_EAT;
					MPI::COMM_WORLD.Send( &msgOut, 1, MPI::INT, PHILOSOPHER_THREAD( rightIdx( phil_id, numPhilos ) ), tag );
					waiting[ rightIdx( phil_id, numPhilos ) ] = false;				
					
				}
				
			}
			
			break;
		
		default:
			std::cout << "Master recieved unknown message: '" << msgIn << "'." << std::endl;
			return 1;
		}
		
	}
  }
  else //I'm a philosopher
  {
    int msgOut; // Send "I want to eat" message
	int msgIn;
	int sleepTime;
	
	while( true ) {
		
		// Wait for a lil bit
		sleepTime = rand() % MAX_SLEEP_TIME;
		sleep( sleepTime );
		
		// Let master know I'm hungry
		msgOut = I_WANT_TO_EAT;
		MPI::COMM_WORLD.Send( &msgOut, 1, MPI::INT, MASTER_THREAD, tag );
		
		MPI::COMM_WORLD.Recv( &msgIn, 1, MPI::INT, MASTER_THREAD, tag, status );
		
		if( msgIn == YOU_CAN_NOT_EAT ) {
			
			/* If I can't eat just repeat previous logic */
			continue;
		} 
		
		if( msgIn != YOU_CAN_EAT ) {
			std::cout << "!!!!!! Recieved unknown message: " << msgIn << std::endl;
			
			if( DEBUG ) {
				return 1;
			} else {
				continue;
			}
		}
		
		// Wait for a lil bit
		sleepTime = rand() % MAX_SLEEP_TIME;
		sleep( sleepTime );
		
		// Let master know I'm full
		msgOut = IM_DONE_EATING;
		MPI::COMM_WORLD.Send( &msgOut, 1, MPI::INT, MASTER_THREAD, tag );
		
	} /* while */
	
  }

  //  Terminate MPI.
  MPI::Finalize ( );
  return 0;
}
