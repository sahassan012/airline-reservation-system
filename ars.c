
#include "ars.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

// flight info struct
struct flight_info {
  int next_tid; // +1 everytime
  int nr_booked; // booked <= seats
  pthread_mutex_t lock; //lock
  pthread_cond_t cond; //condition variable
  struct ticket tickets[]; // all issued tickets of this flight
};

int __nr_flights = 0;
int __nr_seats = 0;
struct flight_info ** flights = NULL;

//
//
// ticket_cmp
//
static int ticket_cmp(const void * p1, const void * p2)
{
  const uint64_t v1 = *(const uint64_t *)p1;
  const uint64_t v2 = *(const uint64_t *)p2;
  if (v1 < v2)
    return -1;
  else if (v1 > v2)
    return 1;
  else
    return 0;
}

// 
// 
// tickets_sort
//
void tickets_sort(struct ticket * ts, int n)
{
  qsort(ts, n, sizeof(*ts), ticket_cmp);
}

//
//
// ars_init
//
void ars_init(int nr_flights, int nr_seats_per_flight)
{
  flights = malloc(sizeof(*flights) * nr_flights);
  for (int i = 0; i < nr_flights; i++) {
    flights[i] = calloc(1, sizeof(flights[i][0]) + (sizeof(struct ticket) * nr_seats_per_flight));
    flights[i]->next_tid = 1;
    pthread_mutex_init(&(flights[i]->lock), NULL);
    pthread_cond_init(&(flights[i]->cond), NULL);
  }
  __nr_flights = nr_flights;
  __nr_seats = nr_seats_per_flight;
}

// 
//
// book_flight
//
int book_flight(short user_id, short flight_number)
{
  // Invalid flight number
  if (flight_number >= __nr_flights)
    return -1;
  
  struct flight_info * fi = flights[flight_number];
   
  // Flight is full
  pthread_mutex_lock(&(fi->lock));
  if (fi->nr_booked >= __nr_seats) { 
    pthread_cond_signal(&(fi->cond));
    pthread_mutex_unlock(&(fi->lock));
    return -1;
  }
  
  // book now
  int tid = fi->next_tid++;
  fi->tickets[fi->nr_booked].uid = user_id;
  fi->tickets[fi->nr_booked].fid = flight_number;
  fi->tickets[fi->nr_booked].tid = tid;
  fi->nr_booked++;
  
  pthread_cond_signal(&(fi->cond));
  pthread_mutex_unlock(&(fi->lock));
  return tid;
}

// 
//
// search_ticket: 
// 	a helper function for cancel/change
// 	search a ticket of a flight and return its offset if found
//
static int search_ticket(struct flight_info * fi, short user_id, int ticket_number)
{
  for (int i = 0; i < fi->nr_booked; i++)
    if (fi->tickets[i].uid == user_id && fi->tickets[i].tid == ticket_number)
      return i; // cancelled
  return -1;
}

//
//
// cancel_flight
//
bool cancel_flight(short user_id, short flight_number, int ticket_number)
{
  if (flight_number >= __nr_flights)
    return false;

  struct flight_info * fi = flights[flight_number];
  pthread_mutex_lock(&(fi->lock));
  
  int offset = search_ticket(fi, user_id, ticket_number);
  if (offset >= 0) {
    fi->tickets[offset] = fi->tickets[fi->nr_booked-1];
    fi->nr_booked--;
    pthread_cond_signal(&(fi->cond));
    pthread_mutex_unlock(&(fi->lock));
    return true; // cancelled
  }
  pthread_cond_signal(&(fi->cond));
  pthread_mutex_unlock(&(fi->lock));
  return false; // not found
}

//
//
// change_flight:
//
int change_flight(short user_id, short old_flight_number, int old_ticket_number,
                  short new_flight_number)
{
  // wrong number or no-op
  if (old_flight_number >= __nr_flights ||
      new_flight_number >= __nr_flights ||
      old_flight_number == new_flight_number)
    return -1;

  // Must be done atomically: 
  //  	(1) cancel the old ticket
  //  	(2) book a new ticket
  // 	If any of the two operations cannot be done, nothing should happen

  // wrong flight number 
  if (old_flight_number >= __nr_flights ||
      new_flight_number >= __nr_flights ||
      old_flight_number == new_flight_number){
    return -1;
  }

  struct flight_info * old_fi = flights[old_flight_number];
  struct flight_info * fi = flights[new_flight_number];

  //If new flight # is greater, acquire old flight's lock first
  if (new_flight_number > old_flight_number){	
  	pthread_mutex_lock(&(old_fi->lock));
 	pthread_mutex_lock(&(fi->lock));
  }
  else{ //otherwise acquire new flight's lock first
 	pthread_mutex_lock(&(fi->lock));
  	pthread_mutex_lock(&(old_fi->lock));
  } 
  
  int offset = search_ticket(old_fi, user_id, old_ticket_number);
  if (offset >= 0) {
	
	  if (fi->nr_booked < __nr_seats) {
  		// book new flight
  		int tid = fi->next_tid++;
  		fi->tickets[fi->nr_booked].uid = user_id;
  		fi->tickets[fi->nr_booked].fid = new_flight_number;
  		fi->tickets[fi->nr_booked].tid = tid;
  		fi->nr_booked++;

 		// cancel old ticket
  		old_fi->tickets[offset] = old_fi->tickets[old_fi->nr_booked-1];
  		old_fi->nr_booked--;

		pthread_cond_signal(&(fi->cond));
                pthread_mutex_unlock(&(fi->lock));

		
    		pthread_cond_signal(&(old_fi->cond));
  		pthread_mutex_unlock(&(old_fi->lock));
		return tid;
	}
  }
  
  pthread_cond_signal(&(fi->cond));
  pthread_mutex_unlock(&(fi->lock));
  
  pthread_cond_signal(&(old_fi->cond));
  pthread_mutex_unlock(&(old_fi->lock));
  return -1;
}

// malloc and dump all tickets in the returned array
struct ticket * dump_tickets(int * n_out)
{
  int n = 0;
  for (int i = 0; i < __nr_flights; i++)
    n += flights[i]->nr_booked;

  struct ticket * const buf = malloc(sizeof(*buf) * n);
  assert(buf);
  n = 0;
  for (int i = 0; i < __nr_flights; i++) {
    memcpy(buf+n, flights[i]->tickets, sizeof(*buf) * flights[i]->nr_booked);
    n += flights[i]->nr_booked;
  }
  *n_out = n; // number of tickets
  return buf;
}

int book_flight_can_wait(short user_id, short flight_number)
{

  struct flight_info * fi = flights[flight_number];
  // if full, wait
  pthread_mutex_lock(&(fi->lock));
  while (fi->nr_booked >= __nr_seats) { 
  	pthread_cond_wait(&(fi->cond), &(fi->lock));  
  }
  
  int tid = fi->next_tid++;
  // book now
  fi->tickets[fi->nr_booked].uid = user_id;
  fi->tickets[fi->nr_booked].fid = flight_number;
  fi->tickets[fi->nr_booked].tid = tid;
  fi->nr_booked++;
  
  pthread_mutex_unlock(&(fi->lock));
  return tid;
}
