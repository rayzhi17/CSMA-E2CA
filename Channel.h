/*
	Channel Component
*/

#define DATARATE 11E6 // Data Transmission Rate
#define PHYRATE 1E6

/*
#define SLOT 20E-6 // Empty Slot
#define DIFS 50E-6
#define SIFS 10E-6
#define L_ack 112*/

//Complying with 802.11n
#define SLOT 16e-06
#define DIFS 34e-06
#define SIFS 9e-06
#define LDBPS 256
#define TSYM 4e-06
			
#include "Aux.h"

component Channel : public TypeII
{

	public:
		void Setup();
		void Start();
		void Stop();
			
	public:
		int Nodes;
		int fairShare;
		float error;

		// Connections
		outport [] void out_slot(SLOT_notification &slot);	
		inport void in_packet(Packet &packet);

		// Timers
		Timer <trigger_t> slot_time; // Duration of current slot
		Timer <trigger_t> rx_time; // Time to receive all packets transmitted in current slot
		Timer <trigger_t> cpSampler; // Sampler of the collision probability		
		
		inport inline void NewSlot(trigger_t& t1);
		inport inline void EndReceptionTime(trigger_t& t2);
		inport inline void Sampler(trigger_t& t3);

		Channel () { 
			connect slot_time.to_component,NewSlot; 
			connect rx_time.to_component,EndReceptionTime;
		    connect cpSampler.to_component,Sampler; }

	private:
		int number_of_transmissions_in_current_slot;
		double succ_tx_duration, collision_duration; // Depends on the packet(s) size(s)
		double empty_slot_duration;
		double L_max;
		double TBack;
		int MAC_H, PCLP_PREAMBLE, PCLP_HEADER;
		int aggregation;
		float errorProbability;
		
		//gathering statistics about the collision's evolution in time
     	ofstream slotsInTime;

	public: // Statistics
		double collision_slots, empty_slots, succesful_slots, total_slots;
		double totalBitsSent;
		double aMPDU; //the same as aggregation
		long long int test;
};

void Channel :: Setup()
{

};

void Channel :: Start()
{
	number_of_transmissions_in_current_slot = 0;
	succ_tx_duration = 10E-3;
	collision_duration = 10E-3;
	empty_slot_duration = 9e-06;

	collision_slots = 0;
	empty_slots = 0;
	succesful_slots = 0;
	total_slots = 0;
	test = 0;

	L_max = 0;
	
	MAC_H = 272;
	PCLP_PREAMBLE = 144; 
	PCLP_HEADER = 48;
	
	TBack = 32e-06 + ceil((16 + 256 + 6)/LDBPS) * TSYM;
	totalBitsSent = 0;

	aggregation = 0;
	errorProbability = 0;

	slot_time.Set(SimTime()); // Let's go!
    cpSampler.Set(SimTime() + 1); //To sample CP 1 segs after the start of the simulator	
	
	slotsInTime.open("Results/slotsInTime.txt", ios::app);

};

void Channel :: Stop()
{
	printf("\n\n");
	printf("---- Channel ----\n");
	printf("Slot Status Probabilities (channel point of view): Empty = %f, Succesful = %f, Collision = %f \n",empty_slots/total_slots,succesful_slots/total_slots,collision_slots/total_slots);
	
	slotsInTime.close();
};

void Channel :: Sampler(trigger_t &)
{
    //Statistics for the evolution of Cp
    /*
	if(total_slots) 
	{
	    slotsInTime << SimTime() << " " << collision_slots/total_slots << endl;
	}
	else
	{
	    slotsInTime << SimTime() << " 0" << endl;
	}
	
	cpSampler.Set(SimTime() + 1);
	*/
}

void Channel :: NewSlot(trigger_t &)
{
	//printf("%f ***** NewSlot ****\n",SimTime());

	SLOT_notification slot;

	slot.status = number_of_transmissions_in_current_slot;

	number_of_transmissions_in_current_slot = 0;
	L_max = 0;

	for(int n = 0; n < Nodes; n++) out_slot[n](slot); // We send the SLOT notification to all connected nodes

	rx_time.Set(SimTime());	// To guarantee that the system works correctly. :)
}

void Channel :: EndReceptionTime(trigger_t &)
{
    //Slots are different than frames. We can transmit multiple frames in one long slot through aggregation
	
	if(number_of_transmissions_in_current_slot==0) 
	{
		slot_time.Set(SimTime()+SLOT);
		empty_slots++;
	}
	if(number_of_transmissions_in_current_slot == 1)
	{
		slot_time.Set(SimTime()+succ_tx_duration);
		succesful_slots ++;
		totalBitsSent += aMPDU*(L_max*8);
	}
	if(number_of_transmissions_in_current_slot > 1)
	{
		slot_time.Set(SimTime()+collision_duration);
		collision_slots ++;	
	}

	
	total_slots++;
	
	//Used to plot slots vs. collision probability
	test++;
	
    if((test % 1000 == 0) && (test < 10001))
	{
	        slotsInTime << Nodes << " " <<  test << " " << collision_slots/total_slots << " " << succesful_slots/total_slots << " " << empty_slots/total_slots << endl;
	}
	
}


void Channel :: in_packet(Packet &packet)
{

	if(packet.L > L_max) L_max = packet.L;
	
	aggregation = packet.aggregation;
	aMPDU = aggregation;
	
	errorProbability = rand() % 100 + 1;
	
	if((errorProbability > 0) && (errorProbability <= error))
	{
	    //If the channel error probability is contained inside the system error margin,
	    //then something wrong is going to happen with the transmissions in this slot
	    number_of_transmissions_in_current_slot+=2;
	}else
	{
	    number_of_transmissions_in_current_slot++;
	}
	
	succ_tx_duration = 32e-06 + ceil((16 + aggregation*(32+(L_max*8)+288) + 6)/LDBPS)*TSYM + SIFS + TBack + DIFS + empty_slot_duration;
	collision_duration = succ_tx_duration;
	
	/*succ_tx_duration = ((PCLP_PREAMBLE + PCLP_HEADER)/PHYRATE) + ((aggregation*L_max*8 + MAC_H)/DATARATE) + SIFS + ((PCLP_PREAMBLE + PCLP_HEADER)/PHYRATE) + (L_ack/PHYRATE) + DIFS;
	collision_duration = ((PCLP_PREAMBLE + PCLP_HEADER)/PHYRATE) + ((aggregation*L_max*8 + MAC_H)/DATARATE) + SIFS + DIFS + ((144 + 48 + 112)/PHYRATE);*/
	
	
}

