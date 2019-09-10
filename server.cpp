// client for interactive executor: setup computation, run remote code and query results

#include <iostream>
#include <sstream>

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/credentials.h>

#include "interactiveExecutor.grpc.pb.h"
#include "interactiveExecutor.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using iExec::InteractiveExecutor;
using iExec::SetupRequest;
using iExec::SetupResult;
using iExec::ClientToServer;
using iExec::ClientToServer_MessageType;
using iExec::ClientToServer_MessageType::ClientToServer_MessageType_RUN;
using iExec::ClientToServer_MessageType::ClientToServer_MessageType_PROVIDE_DATA;
using iExec::ClientToServer_MessageType::ClientToServer_MessageType_REQUEST_STATUS;
using iExec::ClientToServer_MessageType::ClientToServer_MessageType_CANCEL;
using iExec::ServerToClient;
using iExec::ServerToClient_MessageType;
using iExec::ServerToClient_MessageType::ServerToClient_MessageType_ACK_RUN;
using iExec::ServerToClient_MessageType::ServerToClient_MessageType_REQUEST_DATA;
using iExec::ServerToClient_MessageType::ServerToClient_MessageType_PROVIDE_STATUS;
using iExec::ServerToClient_MessageType::ServerToClient_MessageType_ACK_CANCEL;
using iExec::OutputRequest;
using iExec::OutputResult;

class InteractiveExecutorServer : public InteractiveExecutor::Service {
	public:
		explicit InteractiveExecutorServer() {
			_currentRun = "";
		}

		Status Setup(ServerContext* context, const SetupRequest* req, SetupResult* res) override {
			std::cout << "server::setup" << std::endl;

			_currentRun = req->runid();
			std::cout << " got request to setup run '" << _currentRun << "'" << std::endl;
		
			std::string response;
			std::stringstream ress(response);
			ress << "setup of " << _currentRun << " done.";
			res->set_response(ress.str());

			std::cout << "server::setup done" << std::endl;
			return Status::OK;
		}

		Status RunInteractively(ServerContext* context, ServerReaderWriter<ServerToClient, ClientToServer>* stream) override {
			std::cout << "server::run_interactively" << std::endl;
			ClientToServer incoming;
			ServerToClient outgoing;

			std::stringstream strst;
			double t_start, t_end;
			double dt = 0.01;
			int iter = 0;
			double current_t;
			double current_input;

			bool running = true;
			while (running) {

				// handle current message from client
				if (stream->Read(&incoming)) {
					switch(incoming.msgtype()) {
						case ClientToServer_MessageType_RUN:
							std::cout << " received RUN" << std::endl;
							t_start = incoming.t_start();
							t_end   = incoming.t_end();
							current_t = t_start;
							iter      = 0;	
							_results.resize((int)(t_end-t_start)/dt);
							
							// acknowledge RUN command
							outgoing.set_msgtype(ServerToClient_MessageType_ACK_RUN);
							strst.str("");
							strst << "running simulation from " << t_start << " to " << t_end;
							outgoing.set_ack_run(strst.str());
							stream->Write(outgoing);

							// request first input data
							outgoing.set_msgtype(ServerToClient_MessageType_REQUEST_DATA);
							outgoing.set_req_time(current_t);
							stream->Write(outgoing);
							
							// skip rest of loop
							continue;

							break;
						case ClientToServer_MessageType_PROVIDE_DATA:
							//std::cout << " received PROVIDE_DATA" << std::endl;

							current_input = incoming.data();

							break;
					case ClientToServer_MessageType_CANCEL:
							std::cout << " received CANCEL" << std::endl;
							running = false;
							continue; // skip rest of loop
            	break;
						default:
							std::cout << "received unknown message type: " << incoming.msgtype() << std::endl;
							break;
					}
				
					// compute current result: simple "amplifier" (output = input*42)
					_results[iter] = current_input * 42.0;

					if (current_t > t_end) {
						running = false;
					} else {
						// advance simulation time
						current_t += dt;
						iter++;

						// request input for next timestamp
						outgoing.set_msgtype(ServerToClient_MessageType_REQUEST_DATA);
						outgoing.set_req_time(current_t);
						stream->Write(outgoing);
					}

				} else {
					running = false;
				}
			}

			std::cout << "server::run_interactively done" << std::endl;
			return Status::OK;
		}

		Status GetOutput(ServerContext* context, const OutputRequest* req, OutputResult* res) override {
      std::cout << "server::get_output" << std::endl;

			// check if run id matches current run
      if (req->runid() != _currentRun) {
				std::cout << "ERROR: got output request for different run: " << req->runid() << std::endl;
				return Status::CANCELLED;
			}

			// return data
			int numResults = _results.size();
			for (int i=0; i<numResults; ++i) {
				res->add_result(_results[i]);
			}

      std::cout << "server::get_output done" << std::endl;
      return Status::OK;
    }		


	private:
		std::string _currentRun;
		std::vector<double> _results;

};


int main(int argc, char** argv) {

	std::string server_address("0.0.0.0:50051");
	InteractiveExecutorServer service;

	ServerBuilder builder;
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);
	std::unique_ptr<Server> server(builder.BuildAndStart());
	std::cout << "Server listening on " << server_address << std::endl;
	server->Wait();

	return 0;
}
