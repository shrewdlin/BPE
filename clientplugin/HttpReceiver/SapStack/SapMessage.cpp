#include "SapMessage.h"
#include <boost/asio.hpp>
#include "Cipher.h"
#include <boost/thread/mutex.hpp>
#include <string>
#include "SapLogHelper.h"

using std::string;
using namespace sdo::common;

static boost::mutex s_mutexSequence;
unsigned int CSapEncoder::sm_dwSeq=0;
CSapEncoder::CSapEncoder()
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->byContext=0;
	pHeader->byPrio=0;
	pHeader->byBodyType=0;
	pHeader->byCharset=0;

    pHeader->byHeadLen=sizeof(SSapMsgHeader);
    pHeader->dwPacketLen=htonl(sizeof(SSapMsgHeader));
    pHeader->byVersion=0x01;
    pHeader->byM=0xFF;
    pHeader->dwCode=0;
    memset(pHeader->bySignature,0,16);
    m_buffer.reset_loc(sizeof(SSapMsgHeader));
}
CSapEncoder::CSapEncoder(unsigned char byIdentifer,unsigned int dwServiceId,unsigned int dwMsgId,unsigned int dwCode)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->byContext=0;
	pHeader->byPrio=0;
	pHeader->byBodyType=0;
	pHeader->byCharset=0;

    pHeader->byHeadLen=sizeof(SSapMsgHeader);
    pHeader->dwPacketLen=htonl(sizeof(SSapMsgHeader));
    pHeader->byVersion=0x01;
    pHeader->byM=0xFF;
    pHeader->dwCode=0;
    memset(pHeader->bySignature,0,16);
    m_buffer.reset_loc(sizeof(SSapMsgHeader));

    pHeader->byIdentifer=byIdentifer;
    pHeader->dwServiceId=htonl(dwServiceId);
    pHeader->dwMsgId=htonl(dwMsgId);
    pHeader->dwCode=htonl(dwCode);
    
}
void CSapEncoder::Reset()
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->byContext=0;
	pHeader->byPrio=0;
	pHeader->byBodyType=0;
	pHeader->byCharset=0;

    pHeader->byHeadLen=sizeof(SSapMsgHeader);
    pHeader->dwPacketLen=htonl(sizeof(SSapMsgHeader));
    pHeader->byVersion=0x01;
    pHeader->byM=0xFF;
    pHeader->dwCode=0;
    memset(pHeader->bySignature,0,16);
    m_buffer.reset_loc(sizeof(SSapMsgHeader));
}
void CSapEncoder::SetService(unsigned char byIdentifer,unsigned int dwServiceId,unsigned int dwMsgId,unsigned int dwCode)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->byIdentifer=byIdentifer;
    pHeader->dwServiceId=htonl(dwServiceId);
    pHeader->dwMsgId=htonl(dwMsgId);
    pHeader->dwCode=htonl(dwCode);
}
void CSapEncoder::SetIdentifier(unsigned char byIdentifer)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->byIdentifer=byIdentifer;
}
void CSapEncoder::SetServiceId(unsigned int dwServiceId)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->dwServiceId=htonl(dwServiceId);
}
void CSapEncoder::SetMsgId(unsigned int dwMsgId)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->dwMsgId=htonl(dwMsgId);
}
void CSapEncoder::SetCode(unsigned int dwCode)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->dwCode=htonl(dwCode);
}

unsigned int CSapEncoder::CreateAndSetSequence()
{
    unsigned int dwSequnece;
    {
        boost::lock_guard<boost::mutex> guard(s_mutexSequence);
        if(++sm_dwSeq==0)
            sm_dwSeq++;
        dwSequnece=sm_dwSeq;
    }
    SetSequence(dwSequnece);
    return dwSequnece;
}
void CSapEncoder::SetSequence(unsigned int dwSequence)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->dwSequence=htonl(dwSequence);
}

void CSapEncoder::SetMsgOption(unsigned int dwOption)
{
    unsigned char * pOption=(unsigned char *)(&dwOption);
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->byContext=pOption[0];
	pHeader->byPrio=pOption[1];
	pHeader->byBodyType=pOption[2];
	pHeader->byCharset=pOption[3];
}
void CSapEncoder::SetExtHeader(const void *pBuffer, unsigned int nLen)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    pHeader->byHeadLen=sizeof(SSapMsgHeader)+nLen;
    memcpy(m_buffer.top(),pBuffer,nLen);
    m_buffer.inc_loc(nLen);
    pHeader->dwPacketLen=htonl(m_buffer.len());
}
void CSapEncoder::SetBody(const void *pBuffer, unsigned int nLen)
{
    while(m_buffer.capacity()<nLen)
    {
        m_buffer.add_capacity();
    }
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    memcpy(m_buffer.top(),pBuffer,nLen);
    m_buffer.inc_loc(nLen);
    pHeader->dwPacketLen=htonl(m_buffer.len());
}
void CSapEncoder::AesEnc(unsigned char abyKey[16])
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    unsigned int nLeft=0x10-(m_buffer.len()-pHeader->byHeadLen)&0x0f;
    while(m_buffer.capacity()<nLeft)
    {
        m_buffer.add_capacity();
    }
    pHeader=(SSapMsgHeader *)m_buffer.base();
    memset(m_buffer.top(),0,nLeft);
    m_buffer.inc_loc(nLeft);
    unsigned char *pbyInBlk=(unsigned char *)m_buffer.base()+pHeader->byHeadLen;
    CCipher::AesEnc(abyKey,pbyInBlk,m_buffer.len()-pHeader->byHeadLen,pbyInBlk);
    pHeader->dwPacketLen=htonl(m_buffer.len());
}
void CSapEncoder::SetSignature(const char * szKey, unsigned int nLen)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    int nMin=(nLen<16?nLen:16);
    memcpy(pHeader->bySignature,szKey,nMin);
    CCipher::Md5(m_buffer.base(),m_buffer.len(),pHeader->bySignature);
}
void CSapEncoder::SetValue(unsigned short wKey,const void* pValue,unsigned int nValueLen)
{
    unsigned int nFactLen=((nValueLen&0x03)!=0?((nValueLen>>2)+1)<<2:nValueLen);
    while(m_buffer.capacity()<nFactLen+4)
    {
        m_buffer.add_capacity();
    }
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    SSapMsgAttribute *pAtti=(SSapMsgAttribute *)m_buffer.top();
    pAtti->wType=htons(wKey);
    pAtti->wLength=htons(nValueLen+sizeof(SSapMsgAttribute));
    memcpy(pAtti->acValue,pValue,nValueLen);
    memset(pAtti->acValue+nValueLen,0,nFactLen-nValueLen);
    m_buffer.inc_loc(sizeof(SSapMsgAttribute)+nFactLen);
    pHeader->dwPacketLen=htonl(m_buffer.len());
}
void CSapEncoder::SetValue(unsigned short wKey, const string &strValue)
{
    SetValue(wKey,strValue.c_str(),strValue.length());
}
void CSapEncoder::SetValue(unsigned short wKey, unsigned int wValue)
{
    int nNetValue=htonl(wValue);
    SetValue(wKey,&nNetValue,4);
}

void CSapEncoder::BeginValue(unsigned short wType)
{
    while(m_buffer.capacity()<sizeof(SSapMsgAttribute))
    {
        m_buffer.add_capacity();
    }
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    SSapMsgAttribute * pAttri=(SSapMsgAttribute *)m_buffer.top();
    pAttri->wType=htons(wType);
    m_buffer.inc_loc(sizeof(SSapMsgAttribute));
    pHeader->dwPacketLen=htonl(m_buffer.len());

    pAttriBlock=(unsigned char *)pAttri;
}
void CSapEncoder::AddValueBloc(const void *pData,unsigned int nLen)
{
    unsigned int nFactLen=((nLen&0x03)!=0?((nLen>>2)+1)<<2:nLen);
    while(m_buffer.capacity()<nFactLen)
    {
        m_buffer.add_capacity();
    }
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_buffer.base();
    memcpy(m_buffer.top(),pData,nLen);
    memset(m_buffer.top()+nLen,0,nFactLen-nLen);
    m_buffer.inc_loc(nFactLen);
    pHeader->dwPacketLen=htonl(m_buffer.len());
}
void CSapEncoder::EndValue()
{
    ((SSapMsgAttribute *)pAttriBlock)->wLength=htons(m_buffer.top()-pAttriBlock);
}



CSapDecoder::CSapDecoder(const void *pBuffer,int nLen)
{
    m_pBuffer=(void *)pBuffer;
    m_nLen=nLen;
}
CSapDecoder::CSapDecoder()
{
    m_pBuffer=0;
    m_nLen=0;
}
void CSapDecoder::Reset(const void *pBuffer,int nLen)
{
    m_pBuffer=(void *)pBuffer;
    m_nLen=nLen;
    m_mapMultiAttri.clear();
}

int CSapDecoder::VerifySignature(const char * szKey, unsigned int nLen)
{
    unsigned char arSignature[16];
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;
    memcpy(arSignature,pHeader->bySignature,16);
    
    int nMin=(nLen<16?nLen:16);
    memcpy(pHeader->bySignature,szKey,nMin);

    CCipher::Md5((const unsigned char *)m_pBuffer,m_nLen,pHeader->bySignature);
    return memcmp(arSignature,pHeader->bySignature,16);
}

void CSapDecoder::AesDec(unsigned char abyKey[16])
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;
    unsigned char *ptrLoc=(unsigned char *)m_pBuffer+pHeader->byHeadLen;
    CCipher::AesDec(abyKey,ptrLoc,m_nLen-pHeader->byHeadLen,ptrLoc);
}

void CSapDecoder::GetService(unsigned char * pIdentifer,unsigned int * pServiceId,unsigned int * pMsgId,unsigned int * pdwCode)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;
    *pIdentifer=pHeader->byIdentifer;
    *pServiceId=ntohl(pHeader->dwServiceId);
    *pMsgId=ntohl(pHeader->dwMsgId);
    *pdwCode=ntohl(pHeader->dwCode);
}
unsigned char CSapDecoder::GetIdentifier()
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;
    return pHeader->byIdentifer;
}
unsigned int CSapDecoder::GetServiceId()
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;
    return ntohl(pHeader->dwServiceId);
}
unsigned int CSapDecoder::GetMsgId()
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;
    return ntohl(pHeader->dwMsgId);
}
unsigned int CSapDecoder::GetCode()
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;
    return ntohl(pHeader->dwCode);
}

unsigned int CSapDecoder::GetSequence()
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;
    return ntohl(pHeader->dwSequence);
}
unsigned int CSapDecoder::GetMsgOption()
{

    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;
    return *((int *)(&pHeader->byContext));
}
void CSapDecoder::GetExtHeader(void **ppBuffer, int * pLen)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;
    *ppBuffer=(unsigned char *)m_pBuffer+sizeof(SSapMsgHeader);
    *pLen=pHeader->byHeadLen-sizeof(SSapMsgHeader);
}
void CSapDecoder::GetBody(void **ppBuffer, unsigned int * pLen)
{
    SSapMsgHeader *pHeader=(SSapMsgHeader *)m_pBuffer;;
    *ppBuffer=(unsigned char *)m_pBuffer+pHeader->byHeadLen;
    *pLen=ntohl(pHeader->dwPacketLen)-pHeader->byHeadLen;
    
}
void CSapDecoder::DecodeBodyAsTLV()
{
    void * pBody;
    unsigned int nBodyLen;
    GetBody(&pBody,&nBodyLen);
    unsigned char *ptrLoc=(unsigned char *)pBody;
    while(ptrLoc<(unsigned char *)pBody+nBodyLen)
    {
        SSapMsgAttribute *pAtti=(SSapMsgAttribute *)ptrLoc;
        unsigned short nLen=ntohs(pAtti->wLength);
        if(nLen==0)
        {
            break;
        }
        m_mapMultiAttri.insert(AttriMultiMap::value_type(ntohs(pAtti->wType),ptrLoc));
        int nFactLen=((nLen&0x03)!=0?((nLen>>2)+1)<<2:nLen);
        ptrLoc+=nFactLen;
    }
}

int CSapDecoder::GetValue(unsigned short wKey,void** pValue, unsigned int * pValueLen) 
{
    AttriMultiMap::const_iterator itr=m_mapMultiAttri.find(wKey);
    if(itr==m_mapMultiAttri.end())
    {
        SS_XLOG(XLOG_DEBUG,"CSapDecodeMsg::%s,type[%d] not found\n",__FUNCTION__,wKey);
        return -1;
    }

    const unsigned char *ptrLoc=itr->second;
    SSapMsgAttribute *pAtti=(SSapMsgAttribute *)ptrLoc;
    *pValueLen=ntohs(pAtti->wLength)-sizeof(SSapMsgAttribute);
    *pValue=pAtti->acValue;
    return 0;
}
int CSapDecoder::GetValue(unsigned short wKey,string & strValue)
{
    void *pData=NULL;
    unsigned int nLen=0;
    if(GetValue(wKey,&pData,&nLen)==-1||nLen<0)
    {
        SS_XLOG(XLOG_DEBUG,"CSapDecodeMsg::%s,type[%d],len[%d] fail!\n",__FUNCTION__,wKey,nLen);
        return -1;
    }
    strValue=string((const char *)pData,nLen);
    return 0;

}
int CSapDecoder::GetValue(unsigned short wKey, unsigned int * pValue) 
{
    void *pData=NULL;
    unsigned int nLen=0;
    if(GetValue(wKey,&pData,&nLen)==-1||nLen!=4)
    {
        SS_XLOG(XLOG_DEBUG,"CSapDecodeMsg::%s,type[%d],len[%d] fail!\n",__FUNCTION__,wKey,nLen);
        return -1;
    }
    *pValue=ntohl(*(int *)pData);
    return 0;
}

void CSapDecoder::GetValues(unsigned short wKey,vector<SSapValueNode> &vecValues)
{
    std::pair<AttriMultiMap::const_iterator, AttriMultiMap::const_iterator> itr_pair = m_mapMultiAttri.equal_range(wKey);
    AttriMultiMap::const_iterator itr;
	for(itr=itr_pair.first; itr!=itr_pair.second;itr++)
	{
	    const unsigned char *ptrLoc=itr->second;
        SSapMsgAttribute *pAtti=(SSapMsgAttribute *)ptrLoc;

        SSapValueNode tmp;
        tmp.nLen=ntohs(pAtti->wLength)-sizeof(SSapMsgAttribute);
        tmp.pLoc=pAtti->acValue;
		vecValues.push_back(tmp);
	}
}
void CSapDecoder::GetValues(unsigned short wKey,vector<string> &vecValues)
{
    std::pair<AttriMultiMap::const_iterator, AttriMultiMap::const_iterator> itr_pair = m_mapMultiAttri.equal_range(wKey);
    AttriMultiMap::const_iterator itr;
	for(itr=itr_pair.first; itr!=itr_pair.second;itr++)
	{
	    const unsigned char *ptrLoc=itr->second;
        SSapMsgAttribute *pAtti=(SSapMsgAttribute *)ptrLoc;

        int nLen=ntohs(pAtti->wLength)-sizeof(SSapMsgAttribute);
        if(nLen>0)
        {
            string strValue=string((const char *)pAtti->acValue,nLen);
	        vecValues.push_back(strValue);
        }
	}
}
void CSapDecoder::GetValues(unsigned short wKey,vector<unsigned int> &vecValues)
{
    std::pair<AttriMultiMap::const_iterator, AttriMultiMap::const_iterator> itr_pair = m_mapMultiAttri.equal_range(wKey);
    AttriMultiMap::const_iterator itr;
	for(itr=itr_pair.first; itr!=itr_pair.second;itr++)
	{
	    const unsigned char *ptrLoc=itr->second;
        SSapMsgAttribute *pAtti=(SSapMsgAttribute *)ptrLoc;

        if(ntohs(pAtti->wLength)-sizeof(SSapMsgAttribute)==4)
        {
	        vecValues.push_back(ntohl(*(unsigned int *)pAtti->acValue));
        }
	}
}


