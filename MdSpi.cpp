#include "MdSpi.h"
#include <map>
#include <iostream>
#include <mutex>
#include <vector>
#include "TdSpi.h"

extern std::map<std::string, std::string> accountConfig_map;//�����˻���Ϣ��map
//ȫ�ֵĻ�����
extern std::mutex m_mutex;

//ȫ�ֵ�requestId
extern int g_nRequestID;

//ȫ�ֲֳֺ�Լ
extern std::vector<std::string> subscribe_inst_vec;

extern TdSpi* g_pUserTDSpi_AI;//ȫ�ֵ�TD�ص����������AI�����������õ�

int nCount = 0;

MdSpi::MdSpi(CThostFtdcMdApi* mdapi):mdapi(mdapi)
{
	//��¼��������
	//�ڻ���˾����
	m_BrokerId = accountConfig_map["brokerId"];
	//�ڻ��˻�
	m_UserId = accountConfig_map["userId"];
	//����
	m_Passwd = accountConfig_map["passwd"];
	//������Ҫ���׵ĺ�Լ
	memset(m_InstId, 0, sizeof(m_InstId));
	strcpy(m_InstId, accountConfig_map["contract"].c_str());
}

MdSpi::~MdSpi()
{
	//�⼸����new�������ڶ���������������������������Ҫɾ��
	if (m_InstIdList_all)
		delete[] m_InstIdList_all;

	if (m_InstIdList_Position_MD)
		delete[] m_InstIdList_Position_MD;

	if (loginField)
		delete loginField;
}

void MdSpi::OnFrontConnected()//ǰ������
{
	std::cerr << "����ǰ���������ϣ������¼" << std::endl;

	ReqUserLogin(m_BrokerId, m_UserId, m_Passwd);

}

//�����¼����
void MdSpi::ReqUserLogin(std::string brokerId, std::string userId, std::string passwd)
{
	loginField = new CThostFtdcReqUserLoginField();
	strcpy(loginField->BrokerID, brokerId.c_str());
	strcpy(loginField->UserID, userId.c_str());
	strcpy(loginField->Password, passwd.c_str());
	//�����brokerid
	//strcpy(loginField->BrokerID, "0000");
	//strcpy(loginField->UserID, "");//BrokerID���˺�������Բ�����Ҳû�£���Ҳ�������ӵ����飬��������ϵĵ������¼
	//strcpy(loginField->Password, "");
	int nResult = mdapi->ReqUserLogin(loginField, GetNextRequestID());
	std::cerr << "�����¼���飺" << ((nResult == 0) ? "�ɹ�" : "ʧ��") << std::endl;
}

void MdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	std::cerr << "OnRspUserLogin �����½�ɹ�! " << std::endl;;
	std::cerr << "�����գ�" << mdapi->GetTradingDay() << std::endl;
	if (pRspInfo->ErrorID == 0) {
		std::cerr << "����ĵ�½�ɹ�," << "����IDΪ" << nRequestID << std::endl;
		/***************************************************************/
		std::cerr << "���Զ�������" << std::endl;

		////�����Լ�������б�
		InsertInstToSubVec(m_InstId);

		////���ġ��������Խ��׵ĺ�Լ������
		//SubscribeMarketData(m_InstId);

		////��ʽһ�����¶�������ʹ��vector�ķ�ʽ
		//if (subscribe_inst_vec.size() > 0)
		//{
		//	//"IF2012,IF2101,IF2103"
		//	std::cerr << "��������ĺ�Լ������" << subscribe_inst_vec.size() << std::endl;
		//	std::cerr << "�гֲ֣���������  " << std::endl;
		//	SubscribeMarketData(subscribe_inst_vec);
		//
		//}
		////���϶�������ʹ��vector�ķ�ʽ


		////��ʽ�������¶�������ʹ��char�ķ�ʽ
		//if (m_InstIdList_Position_MD)
		//{
		//	std::cerr << "m_InstIdList_Position_MD ��С��" << strlen(m_InstIdList_Position_MD) << "," << m_InstIdList_Position_MD << std::endl;
		//	std::cerr << "�гֲ֣���������  " << std::endl;
		//	SubscribeMarketData(m_InstIdList_Position_MD);//�����е�����6��/�룬û�гֲ־Ͳ��ö�������
		//}
		////���϶�������ʹ��char�ķ�ʽ

		////else��䷽ʽһ�ͷ�ʽ������������
		//else
		//{
		//	std::cerr << "��ǰû�гֲ�" << std::endl;
		//}


		////��ʽ�������¶�������ʹ��ֱ������ķ�ʽ
		//SubscribeMarketData("IC2306,IF2301");
		SubscribeMarketData("ni2301");
		////���϶�������ʹ��char�ķ�ʽ


		//��ʽ�ģ��������к�Լ
		//SubscribeMarketData_All();
		//���϶����������к�Լ�ķ�ʽ


		//SubscribeMarketData("IG6666");//д���˺�Լ���붩������û�д�����ʾ��
		//SubscribeMarketData("IC2301");//����ظ����ģ����ᱨ����ͬһ��Լ���ĳ���6�ξ�û�лر��ˡ���ˡ�ÿ�붩�ĺ�Լ���ܳ���6�Ρ�
		//SubscribeMarketData("IC2301");
		//SubscribeMarketData("IC2301");


		//����������Ĭ�Ͻ�ֹ����
		std::cerr << std::endl << std::endl << std::endl << "����Ĭ�Ͻ�ֹ���֣���������ף�������ָ������֣�yxkc,��ֹ���֣�jzkc��" << std::endl;
	}
}

void MdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField* pSpecificInstrument, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	std::cerr << "OnRspSubMarketData : Instrument:" << pSpecificInstrument->InstrumentID << std::endl;

	if (pRspInfo)
		std::cerr << "errorid" << pRspInfo->ErrorID << "ErrorMsg:" << pRspInfo->ErrorMsg << std::endl;
}

void MdSpi::OnRspUserLogout(CThostFtdcUserLogoutField* pUserLogout, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
}



void MdSpi::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField* pSpecificInstrument, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
}

void MdSpi::SubscribeMarketData(char* instIdList)
{
	//char*��vetor
	std::vector<char*>list;
	/*   strtok()�������ַ����ָ��һ����Ƭ�Ρ�����sָ�����ָ���ַ���������delim��Ϊ�ָ��ַ����а����������ַ���
		��strtok()�ڲ���s���ַ����з��ֲ���delim�а����ķָ��ַ�ʱ, ��Ὣ���ַ���Ϊ\0 �ַ���
		�ڵ�һ�ε���ʱ��strtok()����������s�ַ���������ĵ����򽫲���s���ó�NULL��
		ÿ�ε��óɹ��򷵻�ָ�򱻷ָ��Ƭ�ε�ָ�롣*/
	char* token = strtok(instIdList, ",");
	while (token != NULL)
	{
		list.push_back(token);
		token = strtok(NULL, ",");
	}
	unsigned int len = list.size();
	char** ppInstrument = new char* [len];
	for (unsigned i = 0; i < len; i++)
	{
		ppInstrument[i] = list[i];//ָ�븳ֵ��û���·����ڴ�ռ�
	}
	//��������api��SubscribeMarketData
	int nRet = mdapi->SubscribeMarketData(ppInstrument, len);
	std::cerr << "���������飺" << ((nRet == 0) ? "�ɹ�" : "ʧ��") << std::endl;


	delete[] ppInstrument;
}

void MdSpi::SubscribeMarketData(std::string instIdList)
{
	int len = instIdList.size();
	//����len+1��char�Ŀռ䣬����ᱨ��
	char* pInst = new char[len + 1];
	strcpy(pInst, instIdList.c_str());
	//��Ҫ���Ǽ��Ͻ�β��־
	pInst[len] = '\0';


	//SubscribeMarketData(char* instIdList)
	SubscribeMarketData(pInst);
	delete[]pInst;
}

void MdSpi::SubscribeMarketData(std::vector<std::string>& subscribeVec)
{
	int nLen = subscribeVec.size();

	if (nLen > 0)
	{
		char** ppInst = new char* [nLen];
		for (int i = 0; i < nLen; i++)
		{
			ppInst[i] = new char[31] { 0 };
			//memcpy(ppInst[i], subscribeVec[i].c_str(), 31);
			strcpy(ppInst[i], subscribeVec[i].c_str());

		}

		int nResult = mdapi->SubscribeMarketData(ppInst, nLen);
		std::cerr << "�������� " << (nResult == 0 ? ("�ɹ�") : ("ʧ��")) << std::endl;

		for (int i = 0; i < nLen; i++)
			delete[] ppInst[i];

		delete[] ppInst;
	}
}

void MdSpi::SubscribeMarketData_All()
{
	SubscribeMarketData(m_InstIdList_all);
}

//������鱨����Ӧ��һ��2��ˢ��
void MdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* pDepthMarketData)
{
	std::cout << "===========================================" << std::endl;
	std::cout << "�������" << std::endl;
	std::cout << " ������:" << pDepthMarketData->TradingDay << std::endl
		<< "��Լ����:" << pDepthMarketData->InstrumentID << std::endl
		<< "���¼�:" << pDepthMarketData->LastPrice << std::endl
		<< "�ϴν����:" << pDepthMarketData->PreSettlementPrice << std::endl
		<< "������:" << pDepthMarketData->PreClosePrice << std::endl
		<< "����:" << pDepthMarketData->Volume << std::endl
		<< "��ֲ���:" << pDepthMarketData->PreOpenInterest << std::endl
		<< "������ʱ��" << pDepthMarketData->UpdateTime << std::endl
		<< "�����º���" << pDepthMarketData->UpdateMillisec << std::endl
	<< "�����һ��" << pDepthMarketData->BidPrice1 << std::endl
	<< "������һ:" << pDepthMarketData->BidVolume1 << std::endl
	<< "������һ:" << pDepthMarketData->AskPrice1 << std::endl
	<< "������һ:" << pDepthMarketData->AskVolume1 << std::endl
	<< "�����̼�:" << pDepthMarketData->ClosePrice << std::endl
	<< "���վ���:" << pDepthMarketData->AveragePrice << std::endl
	<< "���ν���۸�:" << pDepthMarketData->SettlementPrice << std::endl
	<< "�ɽ����:" << pDepthMarketData->Turnover << std::endl
	<< "�ֲ���:" << pDepthMarketData->OpenInterest << std::endl;

	if (nCount++ < 1)
		g_pUserTDSpi_AI->PlaceOrder(pDepthMarketData->InstrumentID, pDepthMarketData->ExchangeID, 0, 0, 1, pDepthMarketData->LowerLimitPrice);//�������뿪��1��

}

//���ӣ�"IF2102,IF2103",string����ת��Ϊchar *
void MdSpi::setInstIdList_Position_MD(std::string& inst_holding)
{
	int len = inst_holding.length();
	m_InstIdList_Position_MD = new char[len + 1];

	memcpy(m_InstIdList_Position_MD, inst_holding.c_str(), len);
	m_InstIdList_Position_MD[len] = '\0';
}

void MdSpi::InsertInstToSubVec(char* Inst)
{
	//�����ݲ��뵽vector����  subscribe_inst_vec;
	bool findflag = false;
	int len = subscribe_inst_vec.size();
	for (int i = 0; i < len; i++)
	{
		if (strcmp(subscribe_inst_vec[i].c_str(), Inst) == 0)
		{
			findflag = true;
			break;
		}
	}
	//���û���ҵ��Ͳ��붩�ĺ�Լ��vector��
	if (!findflag)
		subscribe_inst_vec.push_back(Inst);
}

/// <summary>
/// ��string���͵����к�Լת��Ϊchar *����
/// </summary>
/// <param name="inst_all"></param>
///string inst_all="IF2012,IF2101,IF2103";
void MdSpi::set_InstIdList_All(std::string& inst_all)
{
	//�ַ����ĳ���
	int nLen = inst_all.size();
	//
	m_InstIdList_all = new char[nLen + 1];
	strcpy(m_InstIdList_all, inst_all.c_str());
	//ǧ���ס���Ͻ�����־����Ҫ����������
	m_InstIdList_all[nLen] = '\0';
	//delete [] m_InstIdList_all;д���������������
}

int MdSpi::GetNextRequestID()
{
	//��m_nNextRequestID���ϻ�����
/*m_mutex.lock();
int nNextID = m_nNextRequestID++;
m_mutex.unlock();*/
//1ԭ���ڹ��캯������ʹ��m_mutex.lock();
//��������ʱ��ʹ�ý���m_mutex.unlock();
std::lock_guard<std::mutex> m_lock(m_mutex);

	int nNextID = g_nRequestID++;

	return g_nRequestID;
}
