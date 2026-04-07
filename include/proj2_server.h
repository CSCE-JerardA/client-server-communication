// Copyright 2026 JC Austin
// I used Python to bridge the gap for my C++

#ifndef PROJ2_SERVER_H_
#define PROJ2_SERVER_H_

#include<iostream>
#include<queue>
#include<string>
#include<cstdint>
#include<semaphore.h>
#include<mutex>


#include<proj2/lib/domain_socket.h>


class Proj2Server : public proj2::UnixDomainDatagramEndpoint{

    public:

        // Constructor for the server
        Proj2Server(const std::string& s_path, uint32_t r, uint32_t s) 
        : proj2::UnixDomainDatagramEndpoint(s_path),
          readers_(r),
          solvers_(s){

        sem_init(&msg_queue_sem_, 0, 0);

          }

        // Destructor for cleaning up semaphore
        ~Proj2Server() {
            sem_destroy(&msg_queue_sem_);
        }

        // Declares our run function to run it properly
        void Run();

        // Getters
        uint32_t getNumSolvers() const {
          return solvers_;
        }

        uint32_t getNumReaders() const {
          return readers_;
        }

        // Setters
        void setNumSolvers(uint32_t s) {
          solvers_ = s;
        }

        void setNumReaders(uint32_t r) {
          readers_ = r;
        }


        std::queue<std::string> msg_queue_;
	std::mutex queue_mutex_;
        sem_t msg_queue_sem_;

       


    private:

        // Creates self variables
        uint32_t readers_;
        uint32_t solvers_;
        uint32_t available_readers_;
        uint32_t available_solvers_;


};

#endif
