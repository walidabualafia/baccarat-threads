#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <pthread.h>

#define NTHREADS  3
#define PLAYER    0
#define BANKER    1
#define CROUPIER  2

#define MAXCARDS  3

typedef enum { Go, Stop, Draw } cmd_t;
typedef enum { Unread, Read   } bufstate_t;


/* function prototypes */
void *player(void *arg);
void *banker(void *arg);
void  croupier(int nrounds);

/* global shared variables */
// mutex lock
pthread_mutex_t lock;

// global command: go/draw/stop
cmd_t cmd_p, cmd_b; 
pthread_cond_t p_cmd_cv, b_cmd_cv; 

// player's hand
int player_hand[MAXCARDS];
pthread_cond_t player_cv;

// banker's hand
int banker_hand[MAXCARDS];
pthread_cond_t banker_cv;

// state of the thread: read/unread
bufstate_t state_crd[NTHREADS];
bufstate_t state_cmd[NTHREADS];




/* player function */
void *player(void *arg) {
  // local variables
  cmd_t cmd_l;
  
  // CONSUME the initial command
  //
  // obtain the mutex lock
  pthread_mutex_lock(&lock);
  // wait until croupier tells the player there's something to read
  while (state_cmd[PLAYER] == Read) {
    pthread_cond_wait(&p_cmd_cv, &lock);
  }
  // save a local copy of the command
  cmd_l = cmd_p;
  // mark the player state as read
  state_cmd[PLAYER] = Read;
  // free the lock
  pthread_mutex_unlock(&lock);

  // when the croupier tells you to go
  while (cmd_l == Go) {
    struct timeval tv;
    // seed the rand
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec + tv.tv_usec + getpid());

    // draw 1
    int first = 1 + rand() % 13;
    // draw 2
    int second = 1 + rand() % 13;

    // PRODUCE player's cards
    //
    // obtain the lock
    pthread_mutex_lock(&lock);
    // place the cards in the player's hand
    player_hand[0] = first;
    player_hand[1] = second;
    // mark the player's cards as unread for the croupier
    state_crd[PLAYER] = Unread;
    // signal to te croupier that the player produced cards
    pthread_cond_signal(&player_cv);
    // free the lock
    pthread_mutex_unlock(&lock);

    // CONSUME any new commands
    pthread_mutex_lock(&lock);

    while (state_cmd[PLAYER] == Read) {
      pthread_cond_wait(&p_cmd_cv, &lock);
    }

    cmd_l = cmd_p;

    state_cmd[PLAYER] = Read;

    pthread_mutex_unlock(&lock);

    if (cmd_l == Draw) {
      int third = 0;

      // draw 3
      third = 1 + rand() % 13;

      // PRODUCE the cards to the croupier
      pthread_mutex_lock(&lock);

      player_hand[2] = third;

      state_crd[PLAYER] = Unread;

      pthread_cond_signal(&player_cv);

      pthread_mutex_unlock(&lock);

      // CONSUME a new command if available
      pthread_mutex_lock(&lock);

      while (state_cmd[PLAYER] == Read) {
        pthread_cond_wait(&p_cmd_cv, &lock);
      }

      cmd_l = cmd_p;

      state_cmd[PLAYER] = Read;

      pthread_mutex_unlock(&lock);
    }
  }

  return NULL;
}




/* banker function */
void *banker(void *arg) {
  // local variables
  cmd_t cmd_l;
  
  // CONSUME the initial command
  pthread_mutex_lock(&lock);

  while (state_cmd[BANKER] == Read) {
    pthread_cond_wait(&b_cmd_cv, &lock);
  }

  cmd_l = cmd_b;

  state_cmd[BANKER] = Read;

  pthread_mutex_unlock(&lock);

  // when the croupier tells you to go
  while (cmd_l == Go) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec + tv.tv_usec + getpid());

    // draw 1
    int first = 1 + rand() % 13;
    // draw 2
    int second = 1 + rand() % 13;

    // PRODUCE banker's cards
    pthread_mutex_lock(&lock);

    banker_hand[0] = first;
    banker_hand[1] = second;

    state_crd[BANKER] = Unread;

    pthread_cond_signal(&banker_cv);

    pthread_mutex_unlock(&lock);

    // CONSUME any new commands
    pthread_mutex_lock(&lock);

    while (state_cmd[BANKER] == Read) {
      pthread_cond_wait(&b_cmd_cv, &lock);
    }

    cmd_l = cmd_b;

    state_cmd[BANKER] = Read;

    pthread_mutex_unlock(&lock);

    if (cmd_l == Draw) {
      int third = 0;

      third = 1 + rand() % 13;

      // PRODUCE the cards to the croupier
      pthread_mutex_lock(&lock);

      banker_hand[2] = third;

      state_crd[BANKER] = Unread;

      pthread_cond_signal(&banker_cv);

      pthread_mutex_unlock(&lock);

      // CONSUME a new command if available
      pthread_mutex_lock(&lock);

      while (state_cmd[BANKER] == Read) {
        pthread_cond_wait(&b_cmd_cv, &lock);
      }

      cmd_l = cmd_b;

      state_cmd[BANKER] = Read;

      pthread_mutex_unlock(&lock);
    }
  }

  return NULL;
}




/* croupier function */
void croupier(int nrounds) {
  int p_win = 0, b_win = 0, ties = 0; // score variables
  int p_first, p_second, p_third;     // player's cards
  int b_first, b_second, b_third;     // banker's cards
  int p_total, b_total;               // player/banker hands

  printf("Beginning %d Rounds...\n", nrounds);
  
  int i;
  // loop over the number of rounds
  for (i = 0; i < nrounds; i++) {
    printf("-------------------------\nRound %d:\n", i+1);

    // PRODUCE a go to the player
    pthread_mutex_lock(&lock);

    cmd_p = Go;

    state_cmd[PLAYER] = Unread;

    pthread_cond_signal(&p_cmd_cv);

    pthread_mutex_unlock(&lock);

    // CONSUME the player's cards
    pthread_mutex_lock(&lock);

    while (state_crd[PLAYER] == Read) {
      pthread_cond_wait(&player_cv, &lock);
    }

    p_first = player_hand[0];
    p_second = player_hand[1];
    p_total = player_hand[0] + player_hand[1];

    state_crd[PLAYER] = Read;

    pthread_mutex_unlock(&lock);

    // check if player drew a face card
    if (p_first > 10 || p_second > 10) {
      printf("Player draws ");
      // check the first card
      if (p_first > 10) {
        if (p_first == 11) {
          printf("J, ");
        } else if (p_first == 12) {
          printf("Q, ");
        } else if (p_first == 13) {
          printf("K, ");
        }
      } else if (p_first <= 10) {
        printf("%d, ", p_first);
      }
      // check the second card
      if (p_second > 10) {
        if (p_second == 11) {
          printf("J\n");
        } else if (p_second == 12) {
          printf("Q\n");
        } else if (p_second == 13) {
          printf("K\n");
        }
      } else if (p_second <= 10) {
        printf("%d\n", p_second);
      }
    } else {
      printf("Player draws %d, %d\n", p_first, p_second);
    }
    
    // PRODUCE a go to the banker
    pthread_mutex_lock(&lock);

    cmd_b = Go;

    state_cmd[BANKER] = Unread;

    pthread_cond_signal(&b_cmd_cv);

    pthread_mutex_unlock(&lock);

    // CONSUME the banker's cards
    pthread_mutex_lock(&lock);

    while (state_crd[BANKER] == Read) {
      pthread_cond_wait(&banker_cv, &lock);
    }

    b_first = banker_hand[0];
    b_second = banker_hand[1];
    b_total = banker_hand[0] + banker_hand[1];

    state_crd[BANKER] = Read;

    pthread_mutex_unlock(&lock);
    
    // check if banker drew a face card
    if (b_first > 10 || b_second > 10) {
      printf("Bank   draws ");
      // check the first card
      if (b_first > 10) {
        if (b_first == 11) {
          printf("J, ");
        } else if (b_first == 12) {
          printf("Q, ");
        } else if (b_first == 13) {
          printf("K, ");
        }
      } else if (b_first <= 10) {
        printf("%d, ", b_first);
      }
      // check the second card
      if (b_second > 10) {
        if (b_second == 11) {
          printf("J\n");
        } else if (b_second == 12) {
          printf("Q\n");
        } else if (b_second == 13) {
          printf("K\n");
        }
      } else if (b_second <= 10) {
        printf("%d\n", b_second);
      }
    } else {
      printf("Bank   draws %d, %d\n", b_first, b_second);
    }

    // if card is 10 or face card, no value
    if (p_first >= 10) {
      p_first = 0;
    }
    if (p_second >= 10) {
      p_second = 0;
    }
    if (b_first >= 10) {
      b_first = 0;
    }
    if (b_second >= 10) {
      b_second = 0;
    }
    
    // if total is double digit, cut to single digit
    if (p_total >= 10) {
      p_total %= 10;
    }
    if (b_total >= 10) {
      b_total %= 10;
    }

    // check rules
    if ((p_total == 8 || p_total == 9) && (b_total == 8 || b_total == 9)) {
      printf("Tie!\n");
      ties++;
      continue;
    } else if ((p_total == 8 || p_total == 9) && !(b_total == 8 || b_total == 9)) {
      printf("Player wins!\n");
      p_win++;
      continue;
    } else if (!(p_total == 8 || p_total == 9) && (b_total == 8 || b_total == 9)) {
      printf("Banker wins!\n");
      b_win++;
      continue;
    } else {
      if (p_total >= 0 && p_total <= 5) {
        // PRODUCE a draw player 
        pthread_mutex_lock(&lock);

        cmd_p = Draw;

        state_cmd[PLAYER] = Unread;

        pthread_cond_signal(&p_cmd_cv);

        pthread_mutex_unlock(&lock);
        
        // CONSUME the player's cards
        pthread_mutex_lock(&lock);

        while (state_crd[PLAYER] == Read) {
          pthread_cond_wait(&player_cv, &lock);
        }

        p_total += player_hand[2];
        p_third = player_hand[2];

        state_crd[PLAYER] = Read;

        pthread_mutex_unlock(&lock);

        // check if player drew a face card
        if (p_third > 10) {
          if (p_third == 11) {
            printf("Player draws J\n");
          } else if (p_third == 12) {
            printf("Player draws Q\n");
          } else if (p_third == 13) {
            printf("Player draws K\n");
          }
        } else {
          printf("Player draws %d\n", p_third);
        }
      } else {
        printf("Player stands\n");
      }
      
      // rule checking
      if ((b_total <= 2) || (b_total == 3 && p_third != 8) || (b_total == 4 && (p_third >= 2 && p_third <= 7)) || (b_total == 5 && (p_third >= 4 && p_third <= 7)) || (b_total == 6 && (p_third == 6 || p_third == 7))) {
        // PRODUCE draw banker
        pthread_mutex_lock(&lock);

        cmd_b = Draw;

        state_cmd[BANKER] = Unread;

        pthread_cond_signal(&b_cmd_cv);

        pthread_mutex_unlock(&lock);
          
        // CONSUME the banker's cards
        pthread_mutex_lock(&lock);

        while (state_crd[BANKER] == Read) {
          pthread_cond_wait(&banker_cv, &lock);
        }

        b_third = banker_hand[2];
        b_total += banker_hand[2];

        state_crd[BANKER] = Read;

        pthread_mutex_unlock(&lock);

        // check if banker drew a face card
        if (b_third > 10) {
          if (b_third == 11) {
            printf("Bank draws J\n");
          } else if (b_third == 12) {
            printf("Bank draws Q\n");
          } else if (b_third == 13) {
            printf("Bank draws K\n");
          }
        } else {
          printf("Bank draws %d\n", b_third);
        }
      }
      else if (b_total == 7) {
        printf("Banker stands\n");
      }
      
      // if total is double digit, cut down to single digit
      if (p_total >= 10) {
        p_total %= 10;
      }
      if (b_total >= 10) {
        b_total %= 10;
      }

      // highest card wins
      if (p_total > b_total) {
        printf("Player wins!\n");
        p_win++;
        continue;
      } else if (b_total > p_total) {
        printf("Bank wins!\n");
        b_win++;
        continue;
      } else {
        printf("Tie!\n");
        ties++;
        continue;
      }
    }
  }

  // PRODUCE a stop
  pthread_mutex_lock(&lock);

  cmd_p = Stop;

  state_cmd[PLAYER] = Unread;

  pthread_cond_signal(&p_cmd_cv);

  pthread_mutex_unlock(&lock);
  
  // PRODUCE a stop
  pthread_mutex_lock(&lock);

  cmd_b = Stop;

  state_cmd[BANKER] = Unread;

  pthread_cond_signal(&b_cmd_cv);

  pthread_mutex_unlock(&lock);

  printf("-------------------------\n");

  // determine the results of the game
  if (p_win > b_win) {
    // player wins
    printf("Results:\n");
    printf("Player : %d\n", p_win);
    printf("Bank   : %d\n", b_win);
    printf("Ties   : %d\n", ties);
    printf("Player wins!\n");
  } else if (b_win > p_win) {
    // bank wins
    printf("Results:\n");
    printf("Player : %d\n", p_win);
    printf("Bank   : %d\n", b_win);
    printf("Ties   : %d\n", ties);
    printf("Bank wins!\n");
  } else {
    // tie
    printf("Results:\n");
    printf("Player : %d\n", p_win);
    printf("Bank   : %d\n", b_win);
    printf("Ties   : %d\n", ties);
    printf("Tie!\n");
  }

  return;
}


int main(int argc, char **argv) {
  pthread_t tid[2];   // thread ids
  int nrounds = 0;    // number of rounds

  state_cmd[PLAYER] = Read;
  state_cmd[BANKER] = Read;
  state_crd[PLAYER] = Read;
  state_crd[BANKER] = Read;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <nrounds>\n", argv[0]);
    exit(1);
  }

  nrounds = atoi(argv[1]);

  if (nrounds <= 0)
    return 0;

  // initialize the mutex
  if ((pthread_mutex_init(&lock, NULL)) != 0) {
    perror("pthread_mutex_init");
  }
  // create player and banker threads
  // create player thread
  if ((pthread_create(&tid[PLAYER], NULL, &player, NULL)) != 0) {
    perror("pthread_create - player");
  }
  // create banker thread
  if ((pthread_create(&tid[BANKER], NULL, &banker, NULL)) != 0) {
    perror("pthread_create - banker");
  }

  // call croupier() to run the game
  croupier(nrounds);

  // join threads and clean up
  // join player thread
  pthread_join(tid[PLAYER], NULL);
  // join banker thread
  pthread_join(tid[BANKER], NULL);
  // destroy the mutex
  pthread_mutex_destroy(&lock);
}
