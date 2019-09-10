// client for interactive executor: setup computation, run remote code and query results

#include <iostream>
#include <list>
#include <cmath>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "interactiveExecutor.grpc.pb.h"
#include "interactiveExecutor.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
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

class InteractiveExecutorClient {
	public:
		InteractiveExecutorClient(std::shared_ptr<Channel> channel)
			: _stub(InteractiveExecutor::NewStub(channel)) {

		}

		bool setup(void) {
			std::cout << "client::setup" << std::endl;
	
			SetupRequest req;
			SetupResult res;

			// setup request
		  req.set_runid("myrun123");

			ClientContext context;
			Status status = _stub->Setup(&context, req, &res);
			if (!status.ok()) {
				std::cout << "Setup rpc failed. Is the server running?" << std::endl;
				return false;
			}
	
			// check result of setup
			std::string response = res.response();
			std::cout << " Setup rpc result: '" << response << "'" << std::endl;

			std::cout << "client::setup done" << std::endl;
			return true;
		}

		bool exec(void) {
			std::cout << "client::exec" << std::endl;
			
			ClientContext context;
			std::shared_ptr<ClientReaderWriter<ClientToServer, ServerToClient> > stream(_stub->RunInteractively(&context));

			ClientToServer outgoing;
			ServerToClient incoming;
			
			std::cout << " sending RUN" << std::endl;
			outgoing.set_msgtype(ClientToServer_MessageType_RUN);
			outgoing.set_t_start(1.0);
			outgoing.set_t_end(2.0);
			stream->Write(outgoing);

			double f=3.0; // Hz

			bool done = false;
			while (!done) {
				if (stream->Read(&incoming)) {

					double req_time = 0.0;

					// handle message from server
					switch(incoming.msgtype()) {
						case ServerToClient_MessageType_ACK_RUN:
							std::cout << "  ACK_RUN from server: \"" << incoming.ack_run() << "\"" << std::endl;
							break;
						case ServerToClient_MessageType_REQUEST_DATA:
							
							req_time = incoming.req_time();
							
							// internal logging of timestamps for timebase of output
							_timestamps.push_back(req_time);

							//std::cout << "provide input data for t=" << req_time << std::endl;
							outgoing.set_msgtype(ClientToServer_MessageType_PROVIDE_DATA);
							outgoing.set_data(sin(2.0*M_PI*f*req_time));
							stream->Write(outgoing);

							break;
					case ServerToClient_MessageType_PROVIDE_STATUS:
          
              break;
						case ServerToClient_MessageType_ACK_CANCEL:
          
              break;
						default:
							std::cout << "unknown message type from server: " << incoming.msgtype() << std::endl;
							break;
					}
				} else {
					done = true;
				}
			}

			stream->WritesDone();
			
			std::cout << "client::exec done" << std::endl;
			return true;
		}

		bool loadResults(void) {
			std::cout << "client::loadResults" << std::endl;						
			
			ClientContext context;

			OutputRequest request;
			OutputResult result;

			request.set_runid("myrun123");
			
			Status status = _stub->GetOutput(&context, request, &result);
			if (!status.ok()) {
				std::cout << "GetOutput rpc failed!" << std::endl;
				return false;
			}
			
			int numResults = result.result_size();
			std::cout << "got results for " << numResults << " timestamps: " << std::endl;
			for (int i=0; i<numResults; ++i) {
				std::cout << _timestamps.front() << " " << result.result(i) << std::endl;
				_timestamps.pop_front();
			}

			std::cout << "client::loadResults done" << std::endl;
			return true;
		}

	private:
		std::unique_ptr<InteractiveExecutor::Stub> _stub;
		std::list<double> _timestamps;

};

int main(int argc, char** argv) {
	
	bool allOk = true;
	
	InteractiveExecutorClient client(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
	
	if (allOk) allOk = client.setup();
	if (allOk) allOk = client.exec();
	if (allOk) allOk = client.loadResults();

	return (allOk ? 0 : -1);
}
