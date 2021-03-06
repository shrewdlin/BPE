#include "CohConnection.h"
#include <boost/bind.hpp>
#include <iostream>
#include <istream>
#include <ostream>
#include "CohServer.h"
#include "CohLogHelper.h"

#ifdef WIN32
#define strncasecmp strnicmp
#endif

namespace sdo{
	namespace coh{
		int CCohConnection::sm_nTimeout=5;
		CCohConnection::CCohConnection(boost::asio::io_service &io_service):
			m_oIoService(io_service),m_socket(io_service),m_resolver(io_service),m_timer(io_service)

		{
		}
		CCohConnection::CCohConnection(ICohClient *pClient,boost::asio::io_service &io_service):
			m_pClient(pClient),m_oIoService(io_service),m_socket(io_service),m_resolver(io_service),m_timer(io_service)
		{
		}
		CCohConnection::~CCohConnection()
		{
			CS_XLOG(XLOG_TRACE,"CCohConnection::%s,connection[%0x]\n",__FUNCTION__,this);
		}


		void CCohConnection::StartReadRequest()
		{
			CS_XLOG(XLOG_DEBUG,"CCohConnection::%s---3,connection[%0x]\n",__FUNCTION__,this);
			m_socket.async_read_some( boost::asio::buffer(m_szRequest,MAX_COH_BUFFER),
					MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HandleReadRequest, shared_from_this(),
							boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred)));

			m_timer.expires_from_now(boost::posix_time::seconds(sm_nTimeout));
			m_timer.async_wait( MakeCohAllocHandler(m_allocTimer,boost::bind(&CCohConnection::HandleRequestTimeout, 
							shared_from_this(),boost::asio::placeholders::error)));
		}
		void CCohConnection::HandleReadRequest(const boost::system::error_code& err,std::size_t dwTransferred)
		{
			CS_XLOG(XLOG_DEBUG,"CCohConnection::%s,connection[%0x],bytes_transferred[%d]\n",__FUNCTION__,this,dwTransferred);
			if (!err)
			{
				m_strRequest+=string(m_szRequest,dwTransferred);
				string::size_type nContent = m_strRequest.find("\r\n\r\n");
				if(nContent==string::npos)
				{
					m_socket.async_read_some(boost::asio::buffer(m_szRequest,MAX_COH_BUFFER),
							MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HandleReadRequest
									, shared_from_this(), boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred)));           
					return; 
				}
				string::size_type found = m_strRequest.find("\r\n");
				int lenbody=-1;

				while( found != string::npos&&found<nContent ) 
				{
					if(strncasecmp(m_strRequest.substr(found,16).c_str(),"\r\nContent-length",16)==0)
					{
						sscanf(m_strRequest.substr(found+16).c_str(),"%*[: ]%d",&lenbody);
						break;
					}
					found = m_strRequest.find("\r\n",found+1);
				}

				int nRest=nContent+4+lenbody-m_strRequest.length();
				CS_XLOG(XLOG_DEBUG,"CCohConnection::%s,connection[%0x],lenbody[%d],nRest[%d]\n",__FUNCTION__,this,lenbody,nRest);
				if(lenbody<=0||nRest<=0)
				{
					OnReceiveRequestCompleted();
				}
				else
				{
					m_socket.async_read_some(boost::asio::buffer(m_szRequest,MAX_COH_BUFFER),
							MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HandleReadRequest
									, shared_from_this(), boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred)));   
				}
			}
			else
			{
				CS_SLOG(XLOG_WARNING,"CCohConnection::"<<__FUNCTION__<<",connection["<<(void *)this<<"],error:" <<err.message()<<"\n");
			}
		}
		void CCohConnection::OnReceiveRequestCompleted()
		{
			CS_XLOG(XLOG_INFO,"CCohConnection::%s,connection[%0x],request:\n%s\n",__FUNCTION__,this,m_strRequest.c_str());
			boost::system::error_code ignore_ec;
			m_timer.cancel(ignore_ec);

			ConnectionAdapter *pAdapter=new ConnectionAdapter;
			pAdapter->connection_=shared_from_this();

            string strIp="";
            unsigned int nPort=0;
            try{
                strIp=m_socket.remote_endpoint().address().to_string();
                nPort=m_socket.remote_endpoint().port();
            }
            catch(boost::system::system_error & ec){
                CS_SLOG(XLOG_WARNING,"CCohConnection::OnReceiveRequestCompleted,code:" <<ec.what()<<"\n");
            }
			ICohServer::GetServer()->OnReceiveRequest(pAdapter,m_strRequest,strIp ,nPort);
		}
		void CCohConnection::DoSendResponse(const string &strResponse)
		{
			CS_XLOG(XLOG_INFO,"CCohConnection::%s,connection[%0x],response:\n%s\n",__FUNCTION__,this,strResponse.c_str());
			m_strResponse=strResponse;

			boost::asio::async_write(m_socket, boost::asio::buffer(m_strResponse),
					MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HandleWriteResponse, shared_from_this(),
							boost::asio::placeholders::error)));
		}
		void CCohConnection::HandleRequestTimeout(const boost::system::error_code& err)
		{
			CS_SLOG(XLOG_DEBUG,"CCohConnection::"<<__FUNCTION__<<",connection["<<(void *)this<<"],code:" <<err.message()<<"\n");
			if(err!=boost::asio::error::operation_aborted)
			{
				boost::system::error_code ignore_ec;
				m_socket.close(ignore_ec);
			}
		}

		void CCohConnection::HandleWriteResponse(const boost::system::error_code& err)
		{
			CS_XLOG(XLOG_DEBUG,"CCohConnection::%s,connection[%0x]\n",__FUNCTION__,this);
			if (!err)
			{
			}
			else
			{
				CS_SLOG(XLOG_WARNING,"CCohConnection::"<<__FUNCTION__<<",connection["<<(void *)this<<"],error:" 
						<<err.message()<<"\n");
			}
		}

		/*----------------------------------*/
		/*----------------------------------*/

		void CCohConnection::StartSendRequest(const string &strIp, unsigned int dwPort,const string & strRequest,int nTimeout)
		{
			CS_XLOG(XLOG_INFO,"CCohConnection::%s,connection[%0x],addr[%s:%d],request:\n%s\n",__FUNCTION__,this,strIp.c_str(),dwPort,strRequest.c_str());
			m_strRequest=strRequest;


			bool bDommain=false;
			for (std::string::const_iterator it=strIp.begin(); it!=strIp.end(); ++it)
			{
				const char c=*it;
				if((c<'0'||c>'9')&&c!='.') 
				{
					bDommain=true;
					break;
				}
			}
			if(bDommain)
			{
				char szBuf[16]={0};
				sprintf(szBuf,"%d",dwPort);
				tcp::resolver::query query(strIp,szBuf);
				m_resolver.async_resolve(query,
						MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HandleResolve, shared_from_this(),
								boost::asio::placeholders::error,
								boost::asio::placeholders::iterator)));
			}
			else
			{
				boost::system::error_code  err;
				boost::asio::ip::address_v4 addrV4=boost::asio::ip::address_v4::from_string(strIp,err);
				if(err)
				{
					CS_XLOG(XLOG_WARNING,"CCohConnection::%s,connection[%0x],invalid addr[%s:%d]\n",__FUNCTION__,this,strIp.c_str(),dwPort);
					
					m_strResponse="HTTP/1.0 555 "+err.message()+"\r\n\r\n";
					CS_SLOG(XLOG_WARNING,"CCohConnection::"<<__FUNCTION__<<",connection["<<(void *)this<<"],error:" 
							<<err.message()<<"\n");
					OnReceiveResponseCompleted();
					return;
				}
				tcp::endpoint endpoint(addrV4,dwPort);
				tcp::resolver::iterator it=tcp::resolver::iterator();
				CS_XLOG(XLOG_DEBUG,"CCohConnection::%s,connection[%0x],addr[%s:%d]\n",__FUNCTION__,this,endpoint.address().to_string() 
						.c_str() ,endpoint.port());
				m_socket.async_connect(endpoint,
						MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HandleConnected, shared_from_this(),
								boost::asio::placeholders::error, it)));
			}
			
			if(nTimeout==0)
			{
				m_timer.expires_from_now(boost::posix_time::seconds(sm_nTimeout));
			}
			else
			{
				m_timer.expires_from_now(boost::posix_time::seconds(nTimeout));
			}
			m_timer.async_wait(MakeCohAllocHandler(m_allocTimer,boost::bind(&CCohConnection::HandleResponseTimeout, 
							shared_from_this(),boost::asio::placeholders::error)));
		}

		void CCohConnection::HandleResolve(const boost::system::error_code& err,tcp::resolver::iterator endpoint_iterator)
		{
			if (!err)
			{
				tcp::endpoint endpoint = *endpoint_iterator;
				CS_XLOG(XLOG_DEBUG,"CCohConnection::%s,connection[%0x],addr[%s:%d]\n",__FUNCTION__,this,endpoint.address().to_string() 
						.c_str() ,endpoint.port());
				m_socket.async_connect(endpoint,
						MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HandleConnected, shared_from_this(),
								boost::asio::placeholders::error, ++endpoint_iterator)));
			}
			else
			{
				m_strResponse="HTTP/1.0 555 "+err.message()+"\r\n\r\n";
				CS_SLOG(XLOG_WARNING,"CCohConnection::"<<__FUNCTION__<<",connection["<<(void *)this<<"],error:" <<err.message()<<"\n");
				OnReceiveResponseCompleted();
			}
		}
		void CCohConnection::HandleConnected(const boost::system::error_code& err,tcp::resolver::iterator endpoint_iterator)
		{
			CS_XLOG(XLOG_DEBUG,"CCohConnection::%s,connection[%0x]\n",__FUNCTION__,this);
			
			if (!err)
			{
				boost::asio::async_write(m_socket, boost::asio::buffer(m_strRequest),
						MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HandleWriteRequest, shared_from_this(),
								boost::asio::placeholders::error)));
			}
			else if (endpoint_iterator != tcp::resolver::iterator())
			{
				boost::system::error_code ignore_ec;
				m_socket.close(ignore_ec);
				tcp::endpoint endpoint = *endpoint_iterator;
				m_socket.async_connect(endpoint,
						MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HandleConnected, shared_from_this(),
								boost::asio::placeholders::error, ++endpoint_iterator)));
			}
			else
			{
				m_strResponse="HTTP/1.0 555 "+err.message()+"\r\n\r\n";
				CS_SLOG(XLOG_WARNING,"CCohConnection::"<<__FUNCTION__<<",connection["<<(void *)this<<"],error:" 
						<<err.message()<<"\n");
				OnReceiveResponseCompleted();
			}
		}
		void CCohConnection::HandleWriteRequest(const boost::system::error_code& err)
		{
			CS_XLOG(XLOG_DEBUG,"CCohConnection::%s,connection[%0x]\n",__FUNCTION__,this);
			if (!err)
			{
                boost::asio::async_read(m_socket, boost::asio::buffer(m_szResponse,MAX_COH_BUFFER),
						MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HhandleReadResponse, shared_from_this(),
								boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred)));        
			}
			else
			{
				m_strResponse="HTTP/1.0 555 "+err.message()+"\r\n\r\n";
				CS_SLOG(XLOG_WARNING,"CCohConnection::"<<__FUNCTION__<<",connection["<<(void *)this<<"],error:" 
						<<err.message()<<"\n");
				OnReceiveResponseCompleted();
			}
		}
		void CCohConnection::HhandleReadResponse(const boost::system::error_code& err,std::size_t dwTransferred)
		{
			CS_XLOG(XLOG_DEBUG,"CCohConnection::%s,connection[%0x],bytes_transferred[%d]\n",__FUNCTION__,this,dwTransferred);
			if (err==boost::asio::error::eof)
			{
				m_strResponse+=string(m_szResponse,dwTransferred);
				OnReceiveResponseCompleted();
			}
			else if(!err)
			{
				m_strResponse+=string(m_szResponse,dwTransferred);
                boost::asio::async_read(m_socket, boost::asio::buffer(m_szResponse,MAX_COH_BUFFER),
						MakeCohAllocHandler(m_allocConn,boost::bind(&CCohConnection::HhandleReadResponse, shared_from_this(),
								boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred)));
			}
			else
			{
				m_strResponse="HTTP/1.0 555 "+err.message()+"\r\n\r\n";
				CS_SLOG(XLOG_WARNING,"CCohConnection::"<<__FUNCTION__<<",connection["<<(void *)this<<"],error:" 
						<<err.message()<<"\n");
				OnReceiveResponseCompleted();
			}

		}

		void CCohConnection::HandleResponseTimeout(const boost::system::error_code& err)
		{
			CS_SLOG(XLOG_DEBUG,"CCohConnection::"<<__FUNCTION__<<",connection["<<(void *)this<<"],code:" <<err.message()<<"\n");
			if(err!=boost::asio::error::operation_aborted)
			{
				boost::system::error_code ignore_ec;
				m_socket.close(ignore_ec);
			}
		}

		void CCohConnection::OnReceiveResponseCompleted()
		{
			boost::system::error_code ignore_ec;
			m_timer.cancel(ignore_ec);
			if(m_pClient!=NULL)
			{
				CS_XLOG(XLOG_INFO,"CCohConnection::%s,connection[%0x],response:\n%s\n",__FUNCTION__,this,m_strResponse.c_str());
				m_pClient->OnReceiveResponse(m_strResponse);
			}
		}

	}
}
