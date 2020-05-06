// a toy Airline Reservations System

#include "ars.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

/************************************************************************************************************/

struct flight_info
{
     int next_tid; // +1 everytime
     int nr_booked; // booked <= seats
     pthread_mutex_t lock1;
     struct ticket tickets[]; // all issued tickets of this flight
};

/************************************************************************************************************/

pthread_mutex_t big_lock;

int __nr_flights = 0;
int __nr_seats = 0;
struct flight_info** flights = NULL;

/************************************************************************************************************/

static int ticket_cmp(const void* p1, const void* p2)
{
     const uint64_t v1 = *(const uint64_t*)p1;
     const uint64_t v2 = *(const uint64_t*)p2;
     if (v1 < v2) return -1;
     else if (v1 > v2) return 1;
     else return 0;
}
/************************************************************************************************************/

void tickets_sort(struct ticket* ts, int n)
{
     qsort(ts, n, sizeof(*ts), ticket_cmp);
}
/************************************************************************************************************/

void ars_init(int nr_flights, int nr_seats_per_flight)
{
     flights = malloc(sizeof(*flights) * nr_flights);
     for (int i = 0; i < nr_flights; i++)
     {
          flights[i] = calloc(1, sizeof(flights[i][0]) + (sizeof(struct ticket) * nr_seats_per_flight));
          flights[i]->next_tid = 1;
          pthread_mutex_init(&flights[i]->lock1, NULL);
     }
     __nr_flights = nr_flights;
     __nr_seats = nr_seats_per_flight;

     pthread_mutex_init(&big_lock, NULL);
}

/************************************************************************************************************/
int book_flight(short user_id, short flight_number)
{
     if (flight_number >= __nr_flights) { return -1; }
     pthread_mutex_lock(&flights[flight_number]->lock1);
     struct flight_info* fi = flights[flight_number];
  
     if (fi->nr_booked >= __nr_seats)
     {
          pthread_mutex_unlock(&flights[flight_number]->lock1);
          return -1;
     }

     int tid = fi->next_tid++;
  
     fi->tickets[fi->nr_booked].uid = user_id;
     fi->tickets[fi->nr_booked].fid = flight_number;
     fi->tickets[fi->nr_booked].tid = tid;
     fi->nr_booked++;
     pthread_mutex_unlock(&flights[flight_number]->lock1);
     return tid;
}

/************************************************************************************************************/

static int search_ticket(struct flight_info * fi, short user_id, int ticket_number)
{
     for (int i = 0; i < fi->nr_booked; i++)
          if (fi->tickets[i].uid == user_id && fi->tickets[i].tid == ticket_number)
               { return i;}

     return -1;
}

/************************************************************************************************************/

pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
bool cancel_flight(short user_id, short flight_number, int ticket_number)
{
     if (flight_number >= __nr_flights) { return false; }
     
     pthread_mutex_lock(&flights[flight_number]->lock1);
     struct flight_info* fi = flights[flight_number];
     int offset = search_ticket(fi, user_id, ticket_number);
     if (offset >= 0)
     {
          pthread_cond_signal(&condition);
          fi->tickets[offset] = fi->tickets[fi->nr_booked - 1];
          fi->nr_booked--;
          pthread_mutex_unlock(&flights[flight_number]->lock1);
          return true;
     }
     
     pthread_mutex_unlock(&flights[flight_number]->lock1);
     return false;
}

/************************************************************************************************************/

void oldFirstUnlock(short new_flight_number, short old_flight_number)
{
     pthread_mutex_unlock(&flights[old_flight_number]->lock1);
     pthread_mutex_unlock(&flights[new_flight_number]->lock1);
     pthread_cond_signal(&condition);
}

/************************************************************************************************************/

void newFirstUnlock(short new_flight_number, short old_flight_number)
{
     pthread_mutex_unlock(&flights[new_flight_number]->lock1);
     pthread_mutex_unlock(&flights[old_flight_number]->lock1);
     pthread_cond_signal(&condition);
}

/************************************************************************************************************/

void oldFirstLock(short new_flight_number, short old_flight_number)
{
     pthread_mutex_lock(&flights[old_flight_number]->lock1);
     pthread_mutex_lock(&flights[new_flight_number]->lock1);
     pthread_cond_signal(&condition);
}

/************************************************************************************************************/

void newFirstLock(short new_flight_number, short old_flight_number)
{
     pthread_mutex_lock(&flights[new_flight_number]->lock1);
     pthread_mutex_lock(&flights[old_flight_number]->lock1);
     pthread_cond_signal(&condition);
}

/************************************************************************************************************/


void checker (bool flag, short new_flight_number, short old_flight_number)
{
     if (flag == true)
     {
          newFirstUnlock(new_flight_number,old_flight_number);
     }
     else
     {
          oldFirstUnlock(new_flight_number,old_flight_number);
     }
}

/************************************************************************************************************/

void checker2(struct flight_info* fi, short user_id, short flight_number, int tid)
{
     fi->tickets[fi->nr_booked].uid = user_id;
     fi->tickets[fi->nr_booked].fid = flight_number;
     fi->tickets[fi->nr_booked].tid = tid;
     fi->nr_booked++;
}

/************************************************************************************************************/
void checker3 (bool flag, short new_flight_number, short old_flight_number)
{
     if (old_flight_number >= new_flight_number)
     {
          newFirstLock(new_flight_number,old_flight_number);
          pthread_cond_signal(&condition);
     }
     else if(old_flight_number < new_flight_number)
     {
          oldFirstLock(new_flight_number,old_flight_number);
          pthread_cond_signal(&condition);
          flag = true;
     }
}
/************************************************************************************************************/

int change_flight(short user_id, short old_flight_number, int old_ticket_number, short new_flight_number)
{
     bool ml = true;
 
     
     if (old_flight_number >= __nr_flights || new_flight_number >= __nr_flights || old_flight_number == new_flight_number){ return -1; }

     
     // two things must be done atomically: (1) cancel the old ticket and (2) book a new ticket
     // if any of the two operations cannot be done, nothing should happen
     // for example, if the new flight has no seat, the existing ticket must remain valid
     // if the old ticket number is invalid, don't acquire a new ticket
     // TODO: your code here
    
     checker3(ml,new_flight_number,old_flight_number);
     
     struct flight_info * fi  =  flights[old_flight_number];
     struct flight_info * fi2 =  flights[new_flight_number];
     short offset = search_ticket(fi, user_id, old_ticket_number);
     
     if(((offset < 0)) || (!(fi2->nr_booked < __nr_seats)))
     {
          checker(ml,new_flight_number,old_flight_number);
          return -1;
     }
     else
     {
          int tid = fi2->next_tid++;
          checker2(fi2, user_id, new_flight_number, tid);
          fi->tickets[offset] = fi->tickets[fi->nr_booked-1];
          fi->nr_booked--;
          checker(ml,new_flight_number,old_flight_number);
          return tid;
     }
}

/************************************************************************************************************/

struct ticket * dump_tickets(int * n_out)
{
     int n = 0;
     for (int i = 0; i < __nr_flights; i++)
     {n += flights[i]->nr_booked;}

     struct ticket * const buf = malloc(sizeof(*buf) * n);
     assert(buf);
     n = 0;
     
     for (int i = 0; i < __nr_flights; i++)
     {
          memcpy(buf+n, flights[i]->tickets, sizeof(*buf) * flights[i]->nr_booked);
          n += flights[i]->nr_booked;
     }
     
     *n_out = n;
     return buf;
}

/************************************************************************************************************/

int book_flight_can_wait(short user_id, short flight_number)
{
     if (flight_number >= __nr_flights) {return -1;}
     
     pthread_mutex_lock(&flights[flight_number]->lock1);
     struct flight_info* fi = flights[flight_number];
  
     for (;fi->nr_booked >= __nr_seats;)
     {
          pthread_cond_wait(&condition, &flights[flight_number]->lock1);
     }
     
     int tid = fi->next_tid++;

     checker2(fi, user_id, flight_number, tid);
     pthread_mutex_unlock(&flights[flight_number]->lock1);
     return tid;
}

/************************************************************************************************************/
