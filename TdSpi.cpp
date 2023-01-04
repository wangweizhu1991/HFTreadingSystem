#include "TdSpi.h"
#include <map>
#include <iostream>
#include <mutex>
#include <string>
#include "Strategy.h"

extern std::map<std::string, std::string> accountConfig_map;//�����˻���Ϣ��map
extern std::mutex m_mutex;
extern Strategy* g_strategy;//������ָ��

extern int g_nRequestID;

//ȫ�ֲֳֺ�Լ
extern std::vector<std::string> subscribe_inst_vec;

TdSpi::TdSpi(CThostFtdcTraderApi* tdapi, CThostFtdcMdApi* pUserApi_md, MdSpi* pUserSpi_md) :
	m_pUserTDApi_trade(tdapi), m_pUserMDApi_trade(pUserApi_md), m_pUserMDSpi_trade(pUserSpi_md)
{
	m_AppId = accountConfig_map["appid"];
	m_AuthCode = accountConfig_map["authcode"];
	m_BrokerId = accountConfig_map["brokerId"];
	m_UserId = accountConfig_map["userId"];
	m_Passwd = accountConfig_map["passwd"];
	m_InstId = accountConfig_map["contract"];
	m_nNextRequestID = 0;
	m_QryOrder_Once = true;

	m_QryTrade_Once = true;
	m_QryDetail_Once = true;
	m_QryTradingAccount_Once = true;
	m_QryPosition_Once = true;
	m_QryInstrument_Once = true;
}

TdSpi::~TdSpi()
{
	Release();
}

void TdSpi::OnFrontConnected()
{
	std::cerr << "OnFrontConnected: " << std::endl;
	static const char* version = m_pUserTDApi_trade->GetApiVersion();
	std::cerr << "��ǰ��CTP API Version: " << version << std::endl;
	ReqAuthenticate();//���д�͸����
}


int TdSpi::ReqAuthenticate()
{
	CThostFtdcReqAuthenticateField req;
	//��ʼ��
	memset(&req, 0, sizeof(req));

	strcpy(req.AppID, m_AppId.c_str());
	//strcpy(req.AppID, "aaaaa");
	strcpy(req.AuthCode, m_AuthCode.c_str());
	//strcpy(req.AuthCode, "jjjjj");
	strcpy(req.BrokerID, m_BrokerId.c_str());
	//strcpy(req.BrokerID,"jjjjj");
	strcpy(req.UserID, m_UserId.c_str());
	//strcpy(req.UserID, "66666");
	std::cerr << "������֤���˻���Ϣ: " << std::endl
		<< " appid: " << req.AppID
		<< " authcode: " << req.AuthCode
		<< " brokerId: " << req.BrokerID
		<< " userId: " << req.UserID << std::endl;
	return m_pUserTDApi_trade->ReqAuthenticate(&req, GetNextRequestID());
}

void TdSpi::OnRspAuthenticate(CThostFtdcRspAuthenticateField* pRspAuthenticateField, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	std::cerr << "OnRspAuthenticate" << std::endl;
	if (pRspInfo)
	{
		if (pRspInfo->ErrorID == 0)
		{
			std::cerr << "��͸���Գɹ�! " << "ErrMsg: " << pRspInfo->ErrorMsg << std::endl;
			ReqUserLogin();
		}
		else
		{
			std::cerr << "��͸����ʧ��! " << "Errorid: : " << pRspInfo->ErrorID << "ErrMsg: " << pRspInfo->ErrorMsg << std::endl;
		}
	}

}

int TdSpi::ReqUserLogin()
{
	std::cerr << "====ReqUserLogin====,�û���¼��..." << std::endl;
	//����һ��CThostFtdcReqUserLoginField
	CThostFtdcReqUserLoginField reqUserLogin;
	//��ʼ��Ϊ0
	memset(&reqUserLogin, 0, sizeof(reqUserLogin));
	//����brokerid,userid,passwd
	strcpy(reqUserLogin.BrokerID, m_BrokerId.c_str());
	//strcpy(reqUserLogin.BrokerID,"0000");//BrokerID������󣬱���errorid��3�����Ϸ���ctp��¼
	strcpy(reqUserLogin.UserID, m_UserId.c_str());
	//strcpy(reqUserLogin.UserID, "00000000");//UserID������󣬱���errorid��3�����Ϸ���ctp��¼
	strcpy(reqUserLogin.Password, m_Passwd.c_str());
	//strcpy(reqUserLogin.Password, "000000");//UserID������󣬱���errorid��3�����Ϸ���ctp��¼

	//��¼
	return m_pUserTDApi_trade->ReqUserLogin(&reqUserLogin, GetNextRequestID());
}

bool TdSpi::IsErrorRspInfo(CThostFtdcRspInfoField* pRspInfo)
{
	// ���ErrorID != 0, ˵���յ��˴������Ӧ
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	if (bResult)
		std::cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << std::endl;
	return bResult;
}

void TdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	std::cerr << "��¼����ص�OnRspUserLogin" << std::endl;
	if (!IsErrorRspInfo(pRspInfo) && pRspUserLogin)
	{
		m_nFrontID = pRspUserLogin->FrontID;
		m_nSessionID = pRspUserLogin->SessionID;
		int nextOrderRef = atoi(pRspUserLogin->MaxOrderRef);

		sprintf_s(orderRef, sizeof(orderRef), "%d", ++nextOrderRef);

		std::cout << "ǰ�ñ��:" << pRspUserLogin->FrontID << std::endl
			<< "�Ự���" << pRspUserLogin->SessionID << std::endl
			<< "��󱨵�����:" << pRspUserLogin->MaxOrderRef << std::endl
			<< "������ʱ�䣺" << pRspUserLogin->SHFETime << std::endl
			<< "������ʱ�䣺" << pRspUserLogin->DCETime << std::endl
			<< "֣����ʱ�䣺" << pRspUserLogin->CZCETime << std::endl
			<< "�н���ʱ�䣺" << pRspUserLogin->FFEXTime << std::endl
			<< "��Դ��ʱ�䣺" << pRspUserLogin->INETime << std::endl
			<< "�����գ�" << m_pUserTDApi_trade->GetTradingDay() << std::endl;
		strcpy(m_cTradingDay, m_pUserTDApi_trade->GetTradingDay());//���ý�������

		std::cout << "--------------------------------------------" << std::endl << std::endl;
	}

	ReqSettlementInfoConfirm();//����ȷ�Ͻ��㵥
}

void TdSpi::ReqSettlementInfoConfirm()
{
	CThostFtdcSettlementInfoConfirmField req;//����
	memset(&req, 0, sizeof(req));//��ʼ��
	strcpy(req.BrokerID, m_BrokerId.c_str());
	//strcpy(req.BrokerID, "0000");//BrokerID���󣬽��㵥ȷ�ϱ���ErrorID=9, ErrorMsg=CTP:�޴�Ȩ��
	strcpy(req.InvestorID, m_UserId.c_str());
	//strcpy(req.InvestorID, "000000");//UserId���󣬽��㵥ȷ�ϱ���ErrorID=9, ErrorMsg=CTP:�޴�Ȩ��
	int iResult = m_pUserTDApi_trade->ReqSettlementInfoConfirm(&req, GetNextRequestID());
	std::cerr << "--->>> Ͷ���߽�����ȷ��: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << std::endl;
}

void TdSpi::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField* pSettlementInfoConfirm, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	//bIsLast--�Ƿ������һ����Ϣ
	if (bIsLast && !IsErrorRspInfo(pRspInfo) && pSettlementInfoConfirm)
	{
		std::cerr << "��Ӧ | ���㵥..." << pSettlementInfoConfirm->InvestorID
			<< "..." << pSettlementInfoConfirm->ConfirmDate << "," <<
			pSettlementInfoConfirm->ConfirmTime << "...ȷ��" << std::endl << std::endl;

		std::cerr << "���㵥ȷ���������״β�ѯ����" << std::endl;
		//�߳�����3�룬��ctp��̨�г������Ӧʱ�䣬Ȼ���ٽ��в�ѯ����
		std::chrono::milliseconds sleepDuration(3 * 1000);
		std::this_thread::sleep_for(sleepDuration);
		ReqQryOrder();//��ѯ����
	}
}

void TdSpi::ReqQryOrder()
{
	CThostFtdcQryOrderField  QryOrderField;//����
	memset(&QryOrderField, 0, sizeof(CThostFtdcQryOrderField));//��ʼ��Ϊ0
	//brokerid����
	//strcpy(QryOrderField.BrokerID, "0000");
	strcpy(QryOrderField.BrokerID, m_BrokerId.c_str());
	//InvestorID����
	//strcpy(QryOrderField.InvestorID, "666666");
	strcpy(QryOrderField.InvestorID, m_UserId.c_str());
	//����api��ReqQryOrder
	m_pUserTDApi_trade->ReqQryOrder(&QryOrderField, GetNextRequestID());
}

void TdSpi::OnRspQryOrder(CThostFtdcOrderField* pOrder, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	std::cerr << "�����ѯ������Ӧ��OnRspQryOrder" << ",pOrder  " << pOrder << std::endl;
	if (!IsErrorRspInfo(pRspInfo) && pOrder)
	{
		std::cerr << "�����ѯ������Ӧ��ǰ�ñ��FrontID��" << pOrder->FrontID << ",�Ự���:" << pOrder->SessionID
			<< ",��������:  " << pOrder->OrderRef << std::endl;
		//���к�Լ
		if (m_QryOrder_Once == true)
		{
			CThostFtdcOrderField* order = new CThostFtdcOrderField();
			memcpy(order, pOrder, sizeof(CThostFtdcOrderField));
			orderList.insert(std::make_pair(pOrder->BrokerOrderSeq, order));



			//bIsLast�Ƿ������һ�ʻر�
			if (bIsLast)
			{
				m_QryOrder_Once = false;
				std::cerr << "���к�Լ�ı�������" << orderList.size() << std::endl;
				std::cerr << "---------------��ӡ������ʼ---------------" << std::endl;
				for (auto iter = orderList.begin(); iter != orderList.end(); iter++)
				{
					std::cerr << "���͹�˾���룺" << (iter->second)->BrokerID << "  " << "Ͷ���ߴ��룺" << (iter->second)->InvestorID << "  "
						<< "�û����룺" << (iter->second)->UserID << "  " << "��Լ���룺" << (iter->second)->InstrumentID << "  "
						<< "��������" << (iter->second)->Direction << "  " << "��Ͽ�ƽ��־��" << (iter->second)->CombOffsetFlag << "  "
						<< "�۸�" << (iter->second)->LimitPrice << "  " << "������" << (iter->second)->VolumeTotalOriginal << "  "
						<< "�������ã�" << (iter->second)->OrderRef << "  " << "�ͻ����룺" << (iter->second)->ClientID << "  "
						<< "����״̬��" << (iter->second)->OrderStatus << "  " << "ί��ʱ�䣺" << (iter->second)->InsertTime << "  "
						<< "������ţ�" << (iter->second)->OrderStatus << "  " << "�����գ�" << (iter->second)->TradingDay << "  "
						<< "�������ڣ�" << (iter->second)->InsertDate << std::endl;

				}
				std::cerr << "---------------��ӡ�������---------------" << std::endl;
				std::cerr << "��ѯ�������������״β�ѯ�ɽ�" << std::endl;


			}
		}

	}
	else//��ѯ����
	{
		m_QryOrder_Once = false;
		std::cerr << "��ѯ����������û�гɽ������״β�ѯ�ɽ�" << std::endl;

	}
	if (bIsLast)
	{
		//�߳�����3�룬��ctp��̨�г������Ӧʱ�䣬Ȼ���ٽ��в�ѯ����
		std::chrono::milliseconds sleepDuration(3 * 1000);
		std::this_thread::sleep_for(sleepDuration);
		ReqQryTrade();
	}
}

void TdSpi::ReqQryTrade()
{
	CThostFtdcQryTradeField tdField;//����
	memset(&tdField, 0, sizeof(tdField));//��ʼ��

	strcpy(tdField.BrokerID, m_BrokerId.c_str());
	//strcpy(tdField.BrokerID,"0000");
	strcpy(tdField.InvestorID, m_UserId.c_str());
	//strcpy(tdField.InvestorID, "888888");
	//���ý���api��ReqQryTrade
	m_pUserTDApi_trade->ReqQryTrade(&tdField, GetNextRequestID());
}

void TdSpi::OnRspQryTrade(CThostFtdcTradeField* pTrade, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{

	std::cerr << "�����ѯ�ɽ��ر���Ӧ��OnRspQryTrade" << " pTrade " << pTrade << std::endl;

	if (!IsErrorRspInfo(pRspInfo) && pTrade)
	{
		//���к�Լ
		if (m_QryTrade_Once == true)
		{
			CThostFtdcTradeField* trade = new CThostFtdcTradeField();//����trade�ṹ��
			memcpy(trade, pTrade, sizeof(CThostFtdcTradeField));//pTrade���Ƹ�trade
			tradeList.push_back(trade);

			if (bIsLast)
			{
				m_QryTrade_Once = false;
				std::cerr << "���к�Լ�ĳɽ�����" << tradeList.size() << std::endl;
				std::cerr << "---------------��ӡ�ɽ���ʼ---------------" << std::endl;
				for (std::vector<CThostFtdcTradeField*>::iterator iter = tradeList.begin(); iter != tradeList.end(); iter++)
				{
					std::cerr << "���͹�˾���룺" << (*iter)->BrokerID << std::endl << "Ͷ���ߴ��룺" << (*iter)->InvestorID << std::endl
						<< "�û����룺" << (*iter)->UserID << std::endl << "�ɽ���ţ�" << (*iter)->TradeID << std::endl
						<< "��Լ���룺" << (*iter)->InstrumentID << std::endl << "��������" << (*iter)->Direction << std::endl
						<< "��Ͽ�ƽ��־��" << (*iter)->OffsetFlag << std::endl << "Ͷ���ױ���־��" << (*iter)->HedgeFlag << std::endl
						<< "�۸�" << (*iter)->Price << std::endl << "������" << (*iter)->Volume << std::endl
						<< "�������ã�" << (*iter)->OrderRef << std::endl << "���ر�����ţ�" << (*iter)->OrderLocalID << std::endl
						<< "�ɽ�ʱ�䣺" << (*iter)->TradeTime << std::endl << "ҵ��Ԫ��" << (*iter)->BusinessUnit << std::endl
						<< "��ţ�" << (*iter)->SequenceNo << std::endl << "���͹�˾�µ���ţ�" << (*iter)->BrokerOrderSeq << std::endl
						<< "�����գ�" << (*iter)->TradingDay << std::endl;
					std::cerr << "---------���ʳɽ�����--------" << std::endl;
				}
				std::cerr << "---------------��ӡ�ɽ����---------------" << std::endl;
				std::cerr << "��ѯ�������������״β�ѯ�ֲ���ϸ" << std::endl;
			}
		}
	}
	else//��ѯ����
	{
		m_QryOrder_Once = false;
		std::cerr << "��ѯ����������û�гɽ������״β�ѯ�ɽ�" << std::endl;

	}
	if (bIsLast)
	{
		//�߳�����3�룬��ctp��̨�г������Ӧʱ�䣬Ȼ���ٽ��в�ѯ����
		std::chrono::milliseconds sleepDuration(3 * 1000);
		std::this_thread::sleep_for(sleepDuration);
		ReqQryInvestorPositionDetail();//��ѯ�ֲ���ϸ
	}
}

void TdSpi::ReqQryInvestorPositionDetail()
{
	CThostFtdcQryInvestorPositionDetailField pdField;//����
	memset(&pdField, 0, sizeof(pdField));//��ʼ��Ϊ0
	strcpy(pdField.BrokerID, m_BrokerId.c_str());
	//strcpy(pdField.BrokerID, "0000");
	//strcpy(pdField.InstrumentID, m_InstId.c_str());

	strcpy(pdField.InvestorID, m_UserId.c_str());

	//strcpy(pdField.InvestorID, "0000");
	//���ý���api��ReqQryInvestorPositionDetail
	m_pUserTDApi_trade->ReqQryInvestorPositionDetail(&pdField, GetNextRequestID());
}

void TdSpi::OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField* pField, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	std::cerr << "�����ѯͶ���ֲ߳���ϸ�ر���Ӧ��OnRspQryInvestorPositionDetail" << " pInvestorPositionDetail " << pField << std::endl;
	if (!IsErrorRspInfo(pRspInfo) && pField)
	{
		//���к�Լ
		if (m_QryDetail_Once == true)//���ڳ���ʼִ��һ��
		{
			//�������к�Լ��ֻ����δƽ�ֵģ��������Ѿ�ƽ�ֵ�
			//����������ǰ�ĳֲּ�¼���浽δƽ������tradeList_NotClosed_Long��tradeList_NotClosed_Short
			//ʹ�ýṹ��CThostFtdcTradeField����Ϊ����ʱ���ֶΣ���CThostFtdcInvestorPositionDetailFieldû��ʱ���ֶ�
			CThostFtdcTradeField* trade = new CThostFtdcTradeField();//����CThostFtdcTradeField *

			strcpy(trade->InvestorID, pField->InvestorID);///Ͷ���ߴ���
			strcpy(trade->InstrumentID, pField->InstrumentID);///��Լ����
			strcpy(trade->ExchangeID, pField->ExchangeID);///����������
			trade->Direction = pField->Direction;//��������
			trade->Price = pField->OpenPrice;//�۸�
			trade->Volume = pField->Volume;//����
			strcpy(trade->TradeDate, pField->OpenDate);//�ɽ�����
			strcpy(trade->TradeID, pField->TradeID);//*********�ɽ����********
			if (pField->Volume > 0)//ɸѡδƽ�ֺ�Լ
			{
				if (trade->Direction == '0')//���뷽��
					positionDetailList_NotClosed_Long.push_back(trade);
				else if (trade->Direction == '1')//��������
					positionDetailList_NotClosed_Short.push_back(trade);
			}
			//�ռ��ֲֺ�Լ�Ĵ���
			bool find_instId = false;
			for (unsigned int i = 0; i < subscribe_inst_vec.size(); i++)
			{
				if (strcmp(subscribe_inst_vec[i].c_str(), trade->InstrumentID) == 0)//��Լ�Ѵ��ڣ��Ѷ��ģ�����򷵻�0
				/*strcmp������string compare(�ַ����Ƚ�)����д�����ڱȽ������ַ��������ݱȽϽ������������������ʽΪstrcmp(str1,str2)����str1=str2���򷵻��㣻��str1<str2���򷵻ظ�������str1>str2���򷵻�������*/
				{
					find_instId = true;
					break;
				}
			}
			if (!find_instId)//��Լδ���Ĺ�
			{
				std::cerr << trade->InstrumentID <<" :�óֲֺ�Լδ���Ĺ������붩���б�" << std::endl;
				subscribe_inst_vec.push_back(trade->InstrumentID);
			}

		}
		//������к�Լ�ĳֲ���ϸ��Ҫ����߽�����һ���Ĳ�ѯReqQryTradingAccount()
		if (bIsLast)
		{
			m_QryDetail_Once = false;
			//�ֲֵĺ�Լ
			std::string inst_holding;
			//
			for (unsigned int i = 0; i < subscribe_inst_vec.size(); i++)
				inst_holding = inst_holding + subscribe_inst_vec[i] + ",";
			//"IF2102,IF2103,"

			inst_holding = inst_holding.substr(0, inst_holding.length() - 1);//ȥ�����Ķ��ţ���λ��0��ʼ��ѡȡlength-1���ַ�
			//"IF2102,IF2103"

			std::cerr << "��������ǰ�ĳֲ��б�:" << inst_holding << ",inst_holding.length()=" << inst_holding.length()
				<< ",subscribe_inst_vec.size()=" << subscribe_inst_vec.size() << std::endl;

			if (inst_holding.length() > 0)
				m_pUserMDSpi_trade->setInstIdList_Position_MD(inst_holding);//���ó�������ǰ�����֣�����Ҫ��������ĺ�Լ

			//size�������������������
			std::cerr << "�˻����к�Լδƽ�ֵ��������µ�������һ�ʿ��Զ�Ӧ���֣�,�൥:" << positionDetailList_NotClosed_Long.size()
				<< "�յ���" << positionDetailList_NotClosed_Short.size() << std::endl;


			std::cerr << "-----------------------------------------δƽ�ֶ൥��ϸ��ӡstart" << std::endl;
			for (std::vector<CThostFtdcTradeField*>::iterator iter = positionDetailList_NotClosed_Long.begin(); iter != positionDetailList_NotClosed_Long.end(); iter++)
			{
				std::cerr << "BrokerID:" << (*iter)->BrokerID << std::endl << "InvestorID:" << (*iter)->InvestorID << std::endl
					<< "InstrumentID:" << (*iter)->InstrumentID << std::endl << "Direction:" << (*iter)->Direction << std::endl
					<< "OpenPrice:" << (*iter)->Price << std::endl << "Volume:" << (*iter)->Volume << std::endl
					<< "TradeDate:" << (*iter)->TradeDate << std::endl << "TradeID:" << (*iter)->TradeID << std::endl;
			}

			std::cerr << "-----------------------------------------δƽ�ֿյ���ϸ��ӡstart" << std::endl;
			for (std::vector<CThostFtdcTradeField*>::iterator iter = positionDetailList_NotClosed_Short.begin(); iter != positionDetailList_NotClosed_Short.end(); iter++)
			{
				std::cerr << "BrokerID:" << (*iter)->BrokerID << std::endl << "InvestorID:" << (*iter)->InvestorID << std::endl
					<< "InstrumentID:" << (*iter)->InstrumentID << std::endl << "Direction:" << (*iter)->Direction << std::endl
					<< "OpenPrice:" << (*iter)->Price << std::endl << "Volume:" << (*iter)->Volume << std::endl
					<< "TradeDate:" << (*iter)->TradeDate << std::endl << "TradeID:" << (*iter)->TradeID << std::endl;
			}
			std::cerr << "---------------��ӡ�ֲ���ϸ���---------------" << std::endl;
			std::cerr << "��ѯ�ֲ���ϸ���������״β�ѯ�˻��ʽ���Ϣ" << std::endl;
		}

	}
	else
	{
		if (m_QryDetail_Once == true)
		{
			m_QryDetail_Once = false;
			std::cerr << "��ѯ�ֲ���ϸ������û�гֲ���ϸ�����״β�ѯ�˻��ʽ�" << std::endl;
		}
	}
	if (bIsLast)
	{
		//�߳�����3�룬��ctp��̨�г������Ӧʱ�䣬Ȼ���ٽ��в�ѯ����
		std::chrono::milliseconds sleepDuration(3 * 1000);
		std::this_thread::sleep_for(sleepDuration);
		ReqQryTradingAccount();//�����ѯ�ʽ��˻�
	}
}

void TdSpi::ReqQryTradingAccount()//�����ѯ�ʽ��˻�
{
	CThostFtdcQryTradingAccountField req;//����req�Ľṹ�����
	memset(&req, 0, sizeof(req));//��ʼ��
	//�����brokerID
	//strcpy(req.BrokerID, "8888");
	strcpy(req.BrokerID, m_BrokerId.c_str());
	strcpy(req.InvestorID, m_UserId.c_str());
	//strcpy(req.InvestorID, "666666");
	//���ý���api��ReqQryTradingAccount
	int iResult = m_pUserTDApi_trade->ReqQryTradingAccount(&req, GetNextRequestID());
	std::cerr << "--->>> �����ѯ�ʽ��˻�: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << std::endl;
}

//��ѯ�ʽ��˻���Ӧ
void TdSpi::OnRspQryTradingAccount(CThostFtdcTradingAccountField* pTradingAccount, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	std::cerr << "�����ѯͶ�����ʽ��˻��ر���Ӧ��OnRspQryTradingAccount" << " pTradingAccount " << pTradingAccount << std::endl;
	if (!IsErrorRspInfo(pRspInfo) && pTradingAccount)
	{

		std::cerr << "Ͷ���߱�ţ�" << pTradingAccount->AccountID
			<< ", ��̬Ȩ�棺�ڳ�Ȩ��" << pTradingAccount->PreBalance
			<< ", ��̬Ȩ�棺�ڻ�����׼����" << pTradingAccount->Balance
			<< ", �����ʽ�" << pTradingAccount->Available
			<< ", ��ȡ�ʽ�" << pTradingAccount->WithdrawQuota
			<< ", ��ǰ��֤���ܶ" << pTradingAccount->CurrMargin
			<< ", ƽ��ӯ����" << pTradingAccount->CloseProfit
			<< ", �ֲ�ӯ����" << pTradingAccount->PositionProfit
			<< ", �����ѣ�" << pTradingAccount->Commission
			<< ", ���ᱣ֤��" << pTradingAccount->FrozenCash
			<< std::endl;
		//���к�Լ
		if (m_QryTradingAccount_Once == true)
		{
			m_QryTradingAccount_Once = false;
		}

		std::cerr << "---------------��ӡ�ʽ��˻���ϸ���---------------" << std::endl;
		std::cerr << "��ѯ�ʽ��˻����������״β�ѯͶ���ֲ߳���Ϣ" << std::endl;
	}
	else
	{
		if (m_QryTradingAccount_Once == true)//������ʼ��ѯһ��
		{
			m_QryTradingAccount_Once = false;
			std::cerr << "��ѯ�ʽ��˻��������״β�ѯͶ���ֲ߳�" << std::endl;
		}
	}
	//�߳�����3�룬��ctp��̨�г������Ӧʱ�䣬Ȼ���ٽ��в�ѯ����
	std::chrono::milliseconds sleepDuration(3 * 1000);
	std::this_thread::sleep_for(sleepDuration);
	ReqQryInvestorPosition_All();//��ѯ���к�Լ�ĳֲ�
}

void TdSpi::ReqQryInvestorPosition_All()//��ѯ���к�Լ�ĳֲ�
{
	CThostFtdcQryInvestorPositionField req;//����req
	memset(&req, 0, sizeof(req));//��ʼ��Ϊ0

	//strcpy(req.BrokerID, "8888");
	//strcpy(req.BrokerID, m_BrokerId.c_str());
	//strcpy(req.InvestorID, m_UserId.c_str());
	//strcpy(req.InvestorID, "0000");
	//��ԼΪ�գ�������ѯ���к�Լ�ĳֲ֣������reqΪ����һ����
	//strcpy(req.InstrumentID, m_InstId.c_str());
	//���ý���api��ReqQryInvestorPosition
	int iResult = m_pUserTDApi_trade->ReqQryInvestorPosition(&req, GetNextRequestID());//���reqΪ�գ������ѯ���к�Լ�ĳֲ֣����д�˾����ĳ����Լ�����ѯ�Ǹ���Լ��
	std::cerr << "--->>> �����ѯͶ���ֲ߳�: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << std::endl;
}

//��ѯ���к�Լ�ĳֲ���Ӧ
void TdSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField* pInvestorPosition,
	CThostFtdcRspInfoField* pRspInfo, int g_nRequestID, bool bIsLast) {
	//cerr << "�����ѯ�ֲ���Ӧ��OnRspQryInvestorPosition " << ",pInvestorPosition  " << pInvestorPosition << endl;
	if (!IsErrorRspInfo(pRspInfo) && pInvestorPosition)
	{

		//�˻������к�Լ
		if (m_QryPosition_Once == true)
		{
			std::cerr << "�����ѯ�ֲ���Ӧ��OnRspQryInvestorPosition " << " pInvestorPosition:  "
				<< pInvestorPosition << std::endl;//������Ѿ�ƽ��û�гֲֵļ�¼
			std::cerr << "��Ӧ  | ��Լ " << pInvestorPosition->InstrumentID << std::endl
				<< " �ֲֶ�շ��� " << pInvestorPosition->PosiDirection << std::endl//2��3��
				// << " ӳ���ķ��� " << MapDirection(pInvestorPosition->PosiDirection-2,false) << endl
				<< " �ֲܳ� " << pInvestorPosition->Position << std::endl
				<< " ���ճֲ� " << pInvestorPosition->TodayPosition << std::endl
				<< " ���ճֲ� " << pInvestorPosition->YdPosition << std::endl
				<< " ��֤�� " << pInvestorPosition->UseMargin << std::endl
				<< " �ֲֳɱ� " << pInvestorPosition->PositionCost << std::endl
				<< " ������ " << pInvestorPosition->OpenVolume << std::endl
				<< " ƽ���� " << pInvestorPosition->CloseVolume << std::endl
				<< " �ֲ����� " << pInvestorPosition->TradingDay << std::endl
				<< " ƽ��ӯ��������ᣩ " << pInvestorPosition->CloseProfitByDate << std::endl
				<< " �ֲ�ӯ�� " << pInvestorPosition->PositionProfit << std::endl
				<< " ���ն���ƽ��ӯ��������ᣩ " << pInvestorPosition->CloseProfitByDate << std::endl//��������ʾ�������ֵ
				<< " ��ʶԳ�ƽ��ӯ��������ƽ��Լ�� " << pInvestorPosition->CloseProfitByTrade << std::endl//�ڽ����бȽ�������
				<< std::endl;


			//�����Լ��Ӧ�ֲ���ϸ��Ϣ�Ľṹ��map
			bool  find_trade_message_map = false;
			for (std::map<std::string, position_field*>::iterator iter = m_position_field_map.begin(); iter != m_position_field_map.end(); iter++)
			{
				if (strcmp((iter->first).c_str(), pInvestorPosition->InstrumentID) == 0)//��Լ�Ѵ���
				{
					find_trade_message_map = true;
					break;
				}
			}
			if (!find_trade_message_map)//��Լ������
			{
				std::cerr << "-----------------------û�������Լ����Ҫ���콻����Ϣ�ṹ��" << std::endl;
				position_field* p_trade_message = new position_field();
				p_trade_message->instId = pInvestorPosition->InstrumentID;
				m_position_field_map.insert(std::pair<std::string, position_field*>(pInvestorPosition->InstrumentID, p_trade_message));
			}
			if (pInvestorPosition->PosiDirection == '2')//�൥
			{
				//��ֺͽ��һ�η���
				//��ȡ�ú�Լ�ĳֲ���ϸ��Ϣ�ṹ�� second; m_map[��]
				position_field* p_tdm = m_position_field_map[pInvestorPosition->InstrumentID];
				p_tdm->LongPosition = p_tdm->LongPosition + pInvestorPosition->Position;
				p_tdm->TodayLongPosition = p_tdm->TodayLongPosition + pInvestorPosition->TodayPosition;
				p_tdm->YdLongPosition = p_tdm->LongPosition - p_tdm->TodayLongPosition;
				p_tdm->LongCloseProfit = p_tdm->LongCloseProfit + pInvestorPosition->CloseProfit;
				p_tdm->LongPositionProfit = p_tdm->LongPositionProfit + pInvestorPosition->PositionProfit;
			}
			else if (pInvestorPosition->PosiDirection == '3')//�յ�
			{
				//��ֺͽ��һ�η���

				position_field* p_tdm = m_position_field_map[pInvestorPosition->InstrumentID];
				p_tdm->ShortPosition = p_tdm->ShortPosition + pInvestorPosition->Position;
				p_tdm->TodayShortPosition = p_tdm->TodayShortPosition + pInvestorPosition->TodayPosition;
				p_tdm->YdShortPosition = p_tdm->ShortPosition - p_tdm->TodayShortPosition;
				p_tdm->ShortCloseProfit = p_tdm->ShortCloseProfit + pInvestorPosition->CloseProfit;
				p_tdm->ShortPositionProfit = p_tdm->ShortPositionProfit + pInvestorPosition->PositionProfit;
			}

			if (bIsLast)
			{
				m_QryPosition_Once = false;
				for (std::map<std::string, position_field*>::iterator iter = m_position_field_map.begin(); iter != m_position_field_map.end(); iter++)
				{
					std::cerr << "------------------------" << std::endl;
					std::cerr << "��Լ���룺" << iter->second->instId << std::endl
						<< "�൥�ֲ�����" << iter->second->LongPosition << std::endl
						<< "�յ��ֲ�����" << iter->second->ShortPosition << std::endl
						<< "�൥���ճֲ֣�" << iter->second->TodayLongPosition << std::endl
						<< "�൥���ճֲ֣�" << iter->second->YdLongPosition << std::endl
						<< "�յ����ճֲ֣�" << iter->second->TodayShortPosition << std::endl
						<< "�յ����ճֲ֣�" << iter->second->YdShortPosition << std::endl
						<< "�൥����ӯ����" << iter->second->LongPositionProfit << std::endl
						<< "�൥ƽ��ӯ����" << iter->second->LongCloseProfit << std::endl
						<< "�յ�����ӯ����" << iter->second->ShortPositionProfit << std::endl
						<< "�յ�ƽ��ӯ����" << iter->second->ShortCloseProfit << std::endl;

					//�˻�ƽ��ӯ��
					m_CloseProfit = m_CloseProfit + iter->second->LongCloseProfit + iter->second->ShortCloseProfit;
					//�˻�����ӯ��
					m_OpenProfit = m_OpenProfit + iter->second->LongPositionProfit + iter->second->ShortPositionProfit;
				}
				std::cerr << "------------------------" << std::endl;
				std::cerr << "�˻�����ӯ�� " << m_OpenProfit << std::endl;
				std::cerr << "�˻�ƽ��ӯ�� " << m_CloseProfit << std::endl;
			}//bisLast


		}
		std::cerr << "---------------��ѯͶ���ֲ߳����---------------" << std::endl;
		std::cerr << "��ѯ�ֲ��������״β�ѯ���к�Լ����" << std::endl;
	}
	else
	{
		if (m_QryPosition_Once == true)
			m_QryPosition_Once = false;
		std::cerr << "��ѯͶ���ֲֳ߳�����û�гֲ֣��״β�ѯ���к�Լ" << std::endl;
	}
	if (bIsLast)
	{
		//�߳�����3�룬��ctp��̨�г������Ӧʱ�䣬Ȼ���ٽ��в�ѯ����
		std::chrono::milliseconds sleepDuration(10 * 1000);
		std::this_thread::sleep_for(sleepDuration);
		ReqQryInstrumetAll();
	}

}
// ��ѯ�����ڻ���Լ
void TdSpi::ReqQryInvestorPosition(char* pInstrument)
{
	CThostFtdcQryInvestorPositionField req;//����req
	memset(&req, 0, sizeof(req));//��ʼ��Ϊ0

	 
	strcpy(req.BrokerID, m_BrokerId.c_str());
	strcpy(req.InvestorID, m_UserId.c_str());

	//��Լ��д����ĺ�Լ����
	strcpy(req.InstrumentID, pInstrument);
	//���ý���api��ReqQryInvestorPosition
	int iResult = m_pUserTDApi_trade->ReqQryInvestorPosition(&req, GetNextRequestID());//reqΪ�գ������ѯ���к�Լ�ĳֲ�
	std::cerr << "--->>> �����ѯͶ���ֲ߳�: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << std::endl;
}

void TdSpi::ReqQryInstrumetAll()//��ѯ���к�Լ
{
	CThostFtdcQryInstrumentField req;//����req
	memset(&req, 0, sizeof(req));//��ʼ��Ϊ0

	//���ý���api��ReqQryInstrument
	int iResult = m_pUserTDApi_trade->ReqQryInstrument(&req, GetNextRequestID());//req�ṹ��Ϊ0����ѯ���к�Լ
	std::cerr << "--->>> �����ѯ��Լ: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << std::endl;
}

void TdSpi::ReqQryInstrumet(char* pInstrument)//��ѯ������Լ
{
	CThostFtdcQryInstrumentField req;//����req
	memset(&req, 0, sizeof(req));//��ʼ��Ϊ0
	strcpy(req.InstrumentID, pInstrument);//��Լ��д����Ĵ��룬��ʾ��ѯ�ú�Լ����Ϣ
	//���ý���api��ReqQryInstrument
	int iResult = m_pUserTDApi_trade->ReqQryInstrument(&req, GetNextRequestID());//
	std::cerr << "--->>> �����ѯ��Լ: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << std::endl;
}

//��ѯ��Լ��Ӧ
void TdSpi::OnRspQryInstrument(CThostFtdcInstrumentField* pInstrument, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	//cerr << "�����ѯ��Լ��Ӧ��OnRspQryInstrument" << ",pInstrument   " << pInstrument->InstrumentID << endl;
	if (!IsErrorRspInfo(pRspInfo) && pInstrument)
	{

		//�˻������к�Լ
		if (m_QryInstrument_Once == true)
		{
			m_Instrument_All = m_Instrument_All + pInstrument->InstrumentID + ",";

			//�������к�Լ��Ϣ��map
			CThostFtdcInstrumentField* pInstField = new CThostFtdcInstrumentField();
			memcpy(pInstField, pInstrument, sizeof(CThostFtdcInstrumentField));
			m_inst_field_map.insert(std::pair<std::string, CThostFtdcInstrumentField*>(pInstrument->InstrumentID, pInstField));

			//���Խ��׵ĺ�Լ
			if (strcmp(m_InstId.c_str(), pInstrument->InstrumentID) == 0)
			{
				std::cerr << "------------" << std::endl;
				std::cerr << "��Ӧ | ��Լ��" << pInstrument->InstrumentID
					<< " ,��Լ���ƣ�" << pInstrument->InstrumentName
					<< " ,��Լ�ڽ��������룺" << pInstrument->ExchangeInstID
					<< " ,��Ʒ���룺" << pInstrument->ProductID
					<< " ,��Ʒ���ͣ�" << pInstrument->ProductClass
					<< " ,��ͷ��֤���ʣ�" << pInstrument->LongMarginRatio
					<< " ,��ͷ��֤���ʣ�" << pInstrument->ShortMarginRatio
					<< " ,��Լ����������" << pInstrument->VolumeMultiple
					<< " ,��С�䶯��λ��" << pInstrument->PriceTick
					<< " ,���������룺" << pInstrument->ExchangeID
					<< " ,������ݣ�" << pInstrument->DeliveryYear
					<< " ,�����£�" << pInstrument->DeliveryMonth
					<< " ,�����գ�" << pInstrument->CreateDate
					<< " ,�����գ�" << pInstrument->ExpireDate
					<< " ,�����գ�" << pInstrument->OpenDate
					<< " ,��ʼ�����գ�" << pInstrument->StartDelivDate
					<< " ,���������գ�" << pInstrument->EndDelivDate
					<< " ,��Լ��������״̬��" << pInstrument->InstLifePhase
					<< " ,��ǰ�Ƿ��ף�" << pInstrument->IsTrading << std::endl;
				std::cerr<<"------------"<< std::endl;
			}

			if (bIsLast)
			{
				m_QryInstrument_Once = false;
				m_Instrument_All = m_Instrument_All.substr(0, m_Instrument_All.length() - 1);
				std::cerr << "m_Instrument_All�Ĵ�С��" << m_Instrument_All.size() << std::endl;
				std::cerr << "map�Ĵ�С����Լ��������" << m_inst_field_map.size() << std::endl;

				//���ֲֺ�Լ��Ϣ���õ�mdspi
				//m_pUserMDSpi_trade->setInstIdList_Position_MD(m_Inst_Postion);


				//����Լ��Ϣ�ṹ���map���Ƶ�������
				g_strategy->set_instPostion_map_stgy(m_inst_field_map);
				std::cerr << "--------------------------�����Լ��Ϣmap������-----------------------" << std::endl;
				//ShowInstMessage();//���ô�ӡȫ����Լ��Ϣ,�����ͻ��ӡȫ�г���Լ��Ϣ
				//����ȫ�г���Լ����TD���У���Ҫ����ȫ�г���Լ����ʱ������
				m_pUserMDSpi_trade->set_InstIdList_All(m_Instrument_All);
				std::cerr << "TD��ʼ����ɣ�����MD" << std::endl;
				m_pUserMDApi_trade->Init();//��ʹ��init�󣬻ᵽMdSpi���棬��������ǰ������"MdSpi::OnFrontConnected()"
			}
		}
	}
	else
	{
		m_QryInstrument_Once = false;
		std::cerr << "��ѯ��Լʧ��" << std::endl;
	}
}

void TdSpi::OnRspQrySettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField* pSettlementInfoConfirm, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
}

void TdSpi::OnRspUserLogout(CThostFtdcUserLogoutField* pUserLogout, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
}



void TdSpi::OnRspQrySettlementInfo(CThostFtdcSettlementInfoField* pSettlementInfo, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
}



void TdSpi::OnRspUserPasswordUpdate(CThostFtdcUserPasswordUpdateField* pUserPasswordUpdate, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
}

void TdSpi::OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField* pDepthMarketData, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
}


void TdSpi::OnRtnOrder(CThostFtdcOrderField* pOrder)
{
	//1�����ȱ��沢�����������
	UpdateOrder(pOrder, orderList);


	//2����ӡ���еı���

	ShowOrderList(orderList);


	////�ж��Ƿ񱾳��򷢳��ı�����

	////if (pOrder->FrontID != m_nFrontID || pOrder->SessionID != m_nSessionID) {

	////	CThostFtdcOrderField* pOld = GetOrder(pOrder->BrokerOrderSeq);
	////	if (pOld == NULL) {
	////		return;
	////	}
	////}

	char* pszStatus = new char[13];
	switch (pOrder->OrderStatus) {
	case THOST_FTDC_OST_AllTraded:
		strcpy(pszStatus, "ȫ���ɽ�");
		break;
	case THOST_FTDC_OST_PartTradedQueueing:
		strcpy(pszStatus, "���ֳɽ�");
		break;
	case THOST_FTDC_OST_NoTradeQueueing:
		strcpy(pszStatus, "δ�ɽ�");
		break;
	case THOST_FTDC_OST_Canceled:
		strcpy(pszStatus, "�ѳ���");
		break;
	case THOST_FTDC_OST_Unknown:
		strcpy(pszStatus, "δ֪");
		break;
	case THOST_FTDC_OST_NotTouched:
		strcpy(pszStatus, "δ����");
		break;
	case THOST_FTDC_OST_Touched:

		strcpy(pszStatus, "�Ѵ���");
		break;
	default:
		strcpy(pszStatus, "");
		break;
	}


	///*printf("order returned,ins: %s, vol: %d, price:%f, orderref:%s,requestid:%d,traded vol: %d,ExchID:%s, OrderSysID:%s,status: %s,statusmsg:%s\n"
	//	, pOrder->InstrumentID, pOrder->VolumeTotalOriginal, pOrder->LimitPrice, pOrder->OrderRef, pOrder->RequestID
	//	, pOrder->VolumeTraded, pOrder->ExchangeID, pOrder->OrderSysID, pszStatus, pOrder->StatusMsg);*/


	////���沢���±�����״̬
	//UpdateOrder(pOrder);
	std::cerr << "BrokerOrderSeq:" << pOrder->BrokerOrderSeq << "  ,OrderRef;" << pOrder->OrderRef << " ,����״̬  " << pszStatus << ",��ƽ��־  " << pOrder->CombOffsetFlag[0] << std::endl;

	//if (pOrder->OrderStatus == '3' || pOrder->OrderStatus == '1')
	//{
	//	CThostFtdcOrderField* pOld = GetOrder(pOrder->BrokerOrderSeq);
	//	if (pOld && pOld->OrderStatus != '6')
	//	{
	//		std::cerr << "onRtnOrder ׼��������:" <<std::endl;
	//		CancelOrder(pOrder);
	//	}
	//}



}

void TdSpi::OnRtnTrade(CThostFtdcTradeField* pTrade)
{
	//	///����
//#define THOST_FTDC_OF_Open '0'
/////ƽ��
//#define THOST_FTDC_OF_Close '1'
/////ǿƽ
//#define THOST_FTDC_OF_ForceClose '2'
/////ƽ��
//#define THOST_FTDC_OF_CloseToday '3'
/////ƽ��
//#define THOST_FTDC_OF_CloseYesterday '4'
/////ǿ��
//#define THOST_FTDC_OF_ForceOff '5'
/////����ǿƽ
//#define THOST_FTDC_OF_LocalForceClose '6'
//
//	typedef char TThostFtdcOffsetFlagType;


		//printf("trade returned,ins: %s, trade vol: %d, trade price:%f, ExchID:%s, OrderSysID:%s,��ƽ:%c\n"
		//	, pTrade->InstrumentID, pTrade->Volume, pTrade->Price, pTrade->ExchangeID, pTrade->OrderSysID, pTrade->OffsetFlag);
	

	//��Ҫ�ж��Ƿ��Ƕ�������
	//1����дһ��FindTrade����
	bool bFind = false;

	bFind = FindTrade(pTrade);

	//2�����û�м�¼�������ɽ����ݣ���дһ������ɽ��ĺ���
	if (!bFind)
	{
		InsertTrade(pTrade);
		ShowTradeList();
	}
	else//bFindΪtrue���ҵ���
		return;



	////�ж��Ƿ�����ش���
	////���������������Ժ󣬳ɽ����ٴ�ˢ��һ��
	//std::set<std::string>::iterator it = m_TradeIDs.find(pTrade->TradeID);
	////�ɽ��Ѵ���
	//if (it != m_TradeIDs.end()) 
	//{
	//	return;
	//}
	////�ɽ�id���������ǵ�set��������

	////�ж��Ƿ񱾳��򷢳��ı�����
	//CThostFtdcOrderField* pOrder = GetOrder(pTrade->BrokerOrderSeq);
	//if (pOrder != NULL) 
	//{
	//	//ֻ�������򷢳��ı�����

	//	//����ɽ���set
	//	{
	//		std::lock_guard<std::mutex> m_lock(m_mutex);//��������֤���set���ݵİ�ȫ
	//		m_TradeIDs.insert(pOrder->TraderID);
	//	}

	//	printf("trade returned,ins: %s, trade vol: %d, trade price:%f, ExchID:%s, OrderSysID:%s\n"
	//		, pTrade->InstrumentID, pTrade->Volume, pTrade->Price, pTrade->ExchangeID, pTrade->OrderSysID);

	//	//g_strategy->OnRtnTrade(pTrade);

	//}
}

void TdSpi::PlaceOrder(const char* pszCode, const char* pszExchangeID, int nDirection, int nOpenClose, int nVolume, double fPrice)
{
	//���������ṹ�����
	CThostFtdcInputOrderField orderField;
	//��ʼ������
	memset(&orderField, 0, sizeof(CThostFtdcInputOrderField));

	//fill the broker and user fields;

	strcpy(orderField.BrokerID, m_BrokerId.c_str());//�������ǵ�brokerid���ṹ�����
	strcpy(orderField.InvestorID, m_UserId.c_str());//�������ǵ�InvestorID���ṹ�����

	//set the Symbol code;
	strcpy(orderField.InstrumentID, pszCode);//�����µ����ڻ���Լ
	CThostFtdcInstrumentField* instField = m_inst_field_map.find(pszCode)->second;
	/*if (instField)
	{
		const char* ExID = instField->ExchangeID;
		strcpy(orderField.ExchangeID, ExID);
	}*/
	strcpy(orderField.ExchangeID, pszExchangeID);//���������룬"SHFE","CFFEX","DCE","CZCE","INE"
	if (nDirection == 0) {
		orderField.Direction = THOST_FTDC_D_Buy;
	}
	else {
		orderField.Direction = THOST_FTDC_D_Sell;
	}

	orderField.LimitPrice = fPrice;//�۸�

	orderField.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;//Ͷ�������������ױ������е�



	orderField.VolumeTotalOriginal = nVolume;//�µ�����

	//--------------1���۸�����----------------
		//�޼۵���
	orderField.OrderPriceType = THOST_FTDC_OPT_LimitPrice;//�����ļ۸���������

	//�м۵���
	//orderField.OrderPriceType=THOST_FTDC_OPT_AnyPrice;
	//�н����м�
	//orderField.OrderPriceType = THOST_FTDC_OPT_FiveLevelPrice;

	//--------------2����������----------------
	//	///����
//#define THOST_FTDC_CC_Immediately '1'
/////ֹ��
//#define THOST_FTDC_CC_Touch '2'
/////ֹӮ
//#define THOST_FTDC_CC_TouchProfit '3'
/////Ԥ��
//#define THOST_FTDC_CC_ParkedOrder '4'
/////���¼۴���������
//#define THOST_FTDC_CC_LastPriceGreaterThanStopPrice '5'
/////���¼۴��ڵ���������
//#define THOST_FTDC_CC_LastPriceGreaterEqualStopPrice '6'
/////���¼�С��������
//#define THOST_FTDC_CC_LastPriceLesserThanStopPrice '7'
/////���¼�С�ڵ���������
//#define THOST_FTDC_CC_LastPriceLesserEqualStopPrice '8'
/////��һ�۴���������
//#define THOST_FTDC_CC_AskPriceGreaterThanStopPrice '9'
/////��һ�۴��ڵ���������
//#define THOST_FTDC_CC_AskPriceGreaterEqualStopPrice 'A'
/////��һ��С��������
//#define THOST_FTDC_CC_AskPriceLesserThanStopPrice 'B'
/////��һ��С�ڵ���������
//#define THOST_FTDC_CC_AskPriceLesserEqualStopPrice 'C'
/////��һ�۴���������
//#define THOST_FTDC_CC_BidPriceGreaterThanStopPrice 'D'
/////��һ�۴��ڵ���������
//#define THOST_FTDC_CC_BidPriceGreaterEqualStopPrice 'E'
/////��һ��С��������
//#define THOST_FTDC_CC_BidPriceLesserThanStopPrice 'F'
/////��һ��С�ڵ���������
//#define THOST_FTDC_CC_BidPriceLesserEqualStopPrice 'H'

	orderField.ContingentCondition = THOST_FTDC_CC_Immediately;//�����Ĵ�������
	//orderField.ContingentCondition = THOST_FTDC_CC_LastPriceGreaterThanStopPrice;//�����Ĵ�������
	//orderField.StopPrice = 5035.0;//



	//--------------3��ʱ������----------------
	//orderField.TimeCondition = THOST_FTDC_TC_IOC;

//	///������ɣ�������
//#define THOST_FTDC_TC_IOC '1'
/////������Ч
//#define THOST_FTDC_TC_GFS '2'
/////������Ч
//#define THOST_FTDC_TC_GFD '3'
/////ָ������ǰ��Ч
//#define THOST_FTDC_TC_GTD '4'
/////����ǰ��Ч
//#define THOST_FTDC_TC_GTC '5'
/////���Ͼ�����Ч
//#define THOST_FTDC_TC_GFA '6'


	//orderField.TimeCondition = THOST_FTDC_TC_GFS;//ʱ������,������Ч,------û���ã��������������н���������֧�ֵı�������
	//orderField.TimeCondition = THOST_FTDC_TC_GTD;//ʱ������,ָ��������Ч,��������֧�ֵı�������
	//orderField.TimeCondition = THOST_FTDC_TC_GTC;//ʱ������,����ǰ��Ч,��������֧�ֵı�������
	//orderField.TimeCondition = THOST_FTDC_TC_GFA;//ʱ������,���Ͼ�����Ч,��������֧�ֵı�������


	//orderField.TimeCondition = THOST_FTDC_TC_IOC;//ʱ��������������ɣ�������
	orderField.TimeCondition = THOST_FTDC_TC_GFD;//ʱ������,������Ч


	//--------------4����������----------------
	orderField.VolumeCondition = THOST_FTDC_VC_AV;//��������

	//orderField.VolumeCondition = THOST_FTDC_VC_MV;//��С����
	//orderField.VolumeCondition = THOST_FTDC_VC_CV;//ȫ������




	strcpy(orderField.GTDDate, m_cTradingDay);//�µ�������



	orderField.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;//ǿƽԭ��

	switch (nOpenClose) {
	case 0:
		orderField.CombOffsetFlag[0] = THOST_FTDC_OF_Open;//����
		break;
	case 1:
		orderField.CombOffsetFlag[0] = THOST_FTDC_OF_Close;//ƽ��
		break;
	case 2:
		orderField.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;//ƽ���
		break;
	case 3:
		orderField.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;//ƽ���
		break;
	}


	//���ý��׵�api��ReqOrderInsert
	int retCode = m_pUserTDApi_trade->ReqOrderInsert(&orderField, GetNextRequestID());

	if (retCode != 0) {
		printf("failed to insert order,instrument: %s, volume: %d, price: %f\n", pszCode, nVolume, fPrice);
	}
}

void TdSpi::CancelOrder(CThostFtdcOrderField* pOrder)
{
	CThostFtdcInputOrderActionField oa;//����һ�������Ľṹ�����
	memset(&oa, 0, sizeof(CThostFtdcInputOrderActionField));//��ʼ�����ֶ�����

	oa.ActionFlag = THOST_FTDC_AF_Delete;//����
	//�����������ֶΣ���ȷ�����ǵı���
	oa.FrontID = pOrder->FrontID;//ǰ�ñ��
	oa.SessionID = pOrder->SessionID;//�Ự
	strcpy(oa.OrderRef, pOrder->OrderRef);//��������

	if (pOrder->ExchangeID[0] != '\0') {
		strcpy(oa.ExchangeID, pOrder->ExchangeID);
	}
	if (pOrder->OrderSysID[0] != '\0') {
		strcpy(oa.OrderSysID, pOrder->OrderSysID);
	}

	strcpy(oa.BrokerID, pOrder->BrokerID);
	strcpy(oa.UserID, pOrder->UserID);
	strcpy(oa.InstrumentID, pOrder->InstrumentID);
	strcpy(oa.InvestorID, pOrder->InvestorID);

	//oa.RequestID = pOrder->RequestID;
	oa.RequestID = GetNextRequestID();
	//���ý���api�ĳ�������
	int nRetCode = m_pUserTDApi_trade->ReqOrderAction(&oa, oa.RequestID);

	char* pszStatus = new char[13];
	switch (pOrder->OrderStatus) {
	case THOST_FTDC_OST_AllTraded:
		strcpy(pszStatus, "ȫ���ɽ�");
		break;
	case THOST_FTDC_OST_PartTradedQueueing:
		strcpy(pszStatus, "���ֳɽ�");
		break;
	case THOST_FTDC_OST_NoTradeQueueing:
		strcpy(pszStatus, "δ�ɽ�");
		break;
	case THOST_FTDC_OST_Canceled:
		strcpy(pszStatus, "�ѳ���");
		break;
	case THOST_FTDC_OST_Unknown:
		strcpy(pszStatus, "δ֪");
		break;
	case THOST_FTDC_OST_NotTouched:
		strcpy(pszStatus, "δ����");
		break;
	case THOST_FTDC_OST_Touched:

		strcpy(pszStatus, "�Ѵ���");
		break;
	default:
		strcpy(pszStatus, "");
		break;
	}
	/*printf("����ing,ins: %s, vol: %d, price:%f, orderref:%s,requestid:%d,traded vol: %d,ExchID:%s, OrderSysID:%s,status: %s,statusmsg:%s\n"
		, pOrder->InstrumentID, pOrder->VolumeTotalOriginal, pOrder->LimitPrice, pOrder->OrderRef, pOrder->RequestID
		, pOrder->VolumeTraded, pOrder->ExchangeID, pOrder->OrderSysID, pszStatus, pOrder->StatusMsg);*/
		//cerr << "TdSpi::CancelOrder ����ing" << pszStatus << endl;
	if (nRetCode != 0) {
		printf("cancel order failed.\n");
	}
	else
	{
		pOrder->OrderStatus = '6';//��6����ʾ����;��
		std::cerr << "TdSpi::CancelOrder ����ing" << pszStatus << std::endl;
		std::cerr << "TdSpi::CancelOrder ״̬��Ϊ pOrder->OrderStatus :" << pOrder->OrderStatus << std::endl;
	}

	UpdateOrder(pOrder);
}

void TdSpi::ShowPosition()
{
}

void TdSpi::ClosePosition()
{
}

void TdSpi::SetAllowOpen(bool isOk)
{
}

CThostFtdcOrderField* TdSpi::GetOrder(int nBrokerOrderSeq)
{
	CThostFtdcOrderField* pOrder = NULL;//����һ�������ṹ��ָ��
	std::lock_guard<std::mutex> m_lock(m_mutex);//����
	std::map<int, CThostFtdcOrderField*>::iterator it = m_Orders.find(nBrokerOrderSeq);//�ö������кŲ��Ҷ���
	//�ҵ���
	if (it != m_Orders.end()) {
		pOrder = it->second;
	}

	return pOrder;
}

//���沢���±�����״̬
bool TdSpi::UpdateOrder(CThostFtdcOrderField* pOrder)
{
	//���͹�˾���µ����к�,����0��ʾ�Ѿ����ܱ���
	if (pOrder->BrokerOrderSeq > 0)
	{
		std::lock_guard<std::mutex> m_lock(m_mutex);//��������֤���ӳ�����ݵİ�ȫ
		//�������������Ƿ����������
		std::map<int, CThostFtdcOrderField*>::iterator it = m_Orders.find(pOrder->BrokerOrderSeq);
		//������ڣ�������Ҫ��������״̬
		if (it != m_Orders.end())
		{
			CThostFtdcOrderField* pOld = it->second;//�ѽṹ���ָ�븳ֵ��pOld
			//ԭ�����Ѿ��رգ�
			char cOldStatus = pOld->OrderStatus;
			switch (cOldStatus)
			{
			case THOST_FTDC_OST_AllTraded://ȫ���ɽ�
			case THOST_FTDC_OST_Canceled://�ѳ���
			case '6'://canceling//�Լ�����ģ��������Ѿ������˳������󣬻���;��
			case THOST_FTDC_OST_Touched://�Ѿ�����
				return false;
			}
			memcpy(pOld, pOrder, sizeof(CThostFtdcOrderField));//���±�����״̬
			std::cerr << "TdSpi::UpdateOrder pOrder->OrderStatus :" << (it->second)->OrderStatus << std::endl;


		}
		//��������ڣ�������Ҫ�������������Ϣ
		else
		{
			CThostFtdcOrderField* pNew = new CThostFtdcOrderField();
			memcpy(pNew, pOrder, sizeof(CThostFtdcOrderField));
			m_Orders.insert(std::pair<int, CThostFtdcOrderField*>(pNew->BrokerOrderSeq, pNew));
		}
		return true;
	}
	//����Ļ��Ͳ��ü��뵽ӳ��
	return false;
}

bool TdSpi::UpdateOrder(CThostFtdcOrderField* pOrder, std::map<int, CThostFtdcOrderField*>& orderMap)
{
	//���͹�˾���µ����к�,����0��ʾ�Ѿ����ܱ���
	if (pOrder->BrokerOrderSeq > 0)
	{
		std::lock_guard<std::mutex> m_lock(m_mutex);//��������֤���ӳ�����ݵİ�ȫ
		//�������������Ƿ����������
		std::map<int, CThostFtdcOrderField*>::iterator it = orderMap.find(pOrder->BrokerOrderSeq);
		//������ڣ�������Ҫ��������״̬
		if (it != orderMap.end())
		{
			CThostFtdcOrderField* pOld = it->second;//�ѽṹ���ָ�븳ֵ��pOld
			//ԭ�����Ѿ��رգ�
			char cOldStatus = pOld->OrderStatus;
			switch (cOldStatus)
			{
			case THOST_FTDC_OST_AllTraded://ȫ���ɽ�
			case THOST_FTDC_OST_Canceled://�ѳ���
			case '6'://canceling//�Լ�����ģ��������Ѿ������˳������󣬻���;��
			case THOST_FTDC_OST_Touched://�Ѿ�����
				return false;
			}
			memcpy(pOld, pOrder, sizeof(CThostFtdcOrderField));//���±�����״̬
			std::cerr << "TdSpi::UpdateOrder pOrder->OrderStatus :" << (it->second)->OrderStatus << std::endl;


		}
		//��������ڣ�������Ҫ�������������Ϣ
		else
		{
			CThostFtdcOrderField* pNew = new CThostFtdcOrderField();
			memcpy(pNew, pOrder, sizeof(CThostFtdcOrderField));
			orderMap.insert(std::pair<int, CThostFtdcOrderField*>(pNew->BrokerOrderSeq, pNew));
		}
		return true;
	}
	//����Ļ��Ͳ��ü��뵽ӳ��
	return false;
}

int TdSpi::GetNextRequestID()
{
	//��m_nNextRequestID���ϻ�����
	/*m_mutex.lock();
	int nNextID = m_nNextRequestID++;
	m_mutex.unlock();*/
	//1ԭ���ڹ��캯������ʹ��m_mutex.lock();
	//��������ʱ��ʹ�ý���m_mutex.unlock();

	std::lock_guard<std::mutex> m_lock(m_mutex);

	int nNextID = m_nNextRequestID++;

	return m_nNextRequestID;
}

void TdSpi::ShowInstMessage()
{
	//std::map<std::string, CThostFtdcInstrumentField*> m_inst_field_map;
	for (std::map<std::string, CThostFtdcInstrumentField*>::iterator iter = m_inst_field_map.begin(); iter != m_inst_field_map.end(); iter++)
	{
		CThostFtdcInstrumentField* pInstrument = iter->second;

		std::cerr << "��Ӧ | ��Լ��" << pInstrument->InstrumentID
			<< "��Լ���ƣ�" << pInstrument->InstrumentName
			<< " ��Լ�ڽ��������룺" << pInstrument->ExchangeInstID
			<< " ��Ʒ���룺" << pInstrument->ProductID
			<< " ��Ʒ���ͣ�" << pInstrument->ProductClass
			<< " ��ͷ��֤���ʣ�" << pInstrument->LongMarginRatio
			<< " ��ͷ��֤���ʣ�" << pInstrument->ShortMarginRatio
			<< " ��Լ����������" << pInstrument->VolumeMultiple
			<< " ��С�䶯��λ��" << pInstrument->PriceTick
			<< " ���������룺" << pInstrument->ExchangeID
			<< " ������ݣ�" << pInstrument->DeliveryYear
			<< " �����£�" << pInstrument->DeliveryMonth
			<< " �����գ�" << pInstrument->CreateDate
			<< " �����գ�" << pInstrument->ExpireDate
			<< " �����գ�" << pInstrument->OpenDate
			<< " ��ʼ�����գ�" << pInstrument->StartDelivDate
			<< " ���������գ�" << pInstrument->EndDelivDate
			<< " ��Լ��������״̬��" << pInstrument->InstLifePhase
			<< " ��ǰ�Ƿ��ף�" << pInstrument->IsTrading << std::endl;
	}
}

bool TdSpi::FindTrade(CThostFtdcTradeField* pTrade)
{
	//ExchangeID //����������
	//	TradeID   //�ɽ����
	//	Direction  //��������
	std::lock_guard<std::mutex> m_lock(m_mutex);//��������֤���set���ݵİ�ȫ
	for (auto it = tradeList.begin(); it != tradeList.end(); it++)
	{
		//�ж��Ƿ��Ѿ�����
		if (strcmp((*it)->ExchangeID, pTrade->ExchangeID) == 0 &&
			strcmp((*it)->TradeID, pTrade->TradeID) == 0 && (*it)->Direction == pTrade->Direction)
			return true;
	}

	return false;
}

void TdSpi::InsertTrade(CThostFtdcTradeField* pTrade)
{
	std::lock_guard<std::mutex> m_lock(m_mutex);//��������֤���set���ݵİ�ȫ
	CThostFtdcTradeField* trade = new CThostFtdcTradeField();//����trade������ѿռ䣬�ǵ���������������Ҫdelete
	memcpy(trade, pTrade, sizeof(CThostFtdcTradeField));//pTrade���Ƹ�trade
	tradeList.push_back(trade);//����¼��
}

void TdSpi::ShowTradeList()
{
	std::lock_guard<std::mutex> m_lock(m_mutex);//��������֤���set���ݵİ�ȫ
	std::cerr << std::endl << std::endl;
	std::cerr << "---------------��ӡ�ɽ���ϸ-------------" << std::endl;
	for (auto iter = tradeList.begin(); iter != tradeList.end(); iter++)
	{
		std::cerr << std::endl << "Ͷ���ߴ��룺" << (*iter)->InvestorID << "  "
			<< "�û����룺" << (*iter)->UserID << "  " << "�ɽ���ţ�" << (*iter)->TradeID << "  "
			<< "��Լ���룺" << (*iter)->InstrumentID << "  " << "��������" << (*iter)->Direction << "  "
			<< "��ƽ��" << (*iter)->OffsetFlag << "  " << "Ͷ��/�ױ�" << (*iter)->HedgeFlag << "  "
			<< "�۸�" << (*iter)->Price << "  " << "������" << (*iter)->Volume << "  "
			<< "�������ã�" << (*iter)->OrderRef << "  " << "���ر�����ţ�" << (*iter)->OrderLocalID << "  "
			<< "�ɽ�ʱ�䣺" << (*iter)->TradeTime << "  " << "ҵ��Ԫ��" << (*iter)->BusinessUnit << "  "
			<< "��ţ�" << (*iter)->SequenceNo << "  " << "���͹�˾�µ���ţ�" << (*iter)->BrokerOrderSeq << "  "
			<< "�����գ�" << (*iter)->TradingDay << std::endl;
	}

	std::cerr << std::endl << std::endl;

}

void TdSpi::ShowOrderList(std::map<int, CThostFtdcOrderField*>& orderList)
{
	if (orderList.size() > 0)
	{
		std::cerr << std::endl << std::endl << std::endl;
		std::cerr << "�ܵı���������" << orderList.size() << std::endl;
		std::cerr << "---------------��ӡ������ʼ---------------" << std::endl;
		for (auto iter = orderList.begin(); iter != orderList.end(); iter++)
		{
			std::cerr << "���͹�˾���룺" << (iter->second)->BrokerID << "  " << "Ͷ���ߴ��룺" << (iter->second)->InvestorID << "  "
				<< "�û����룺" << (iter->second)->UserID << "  " << "��Լ���룺" << (iter->second)->InstrumentID << "  "
				<< "��������" << (iter->second)->Direction << "  " << "��Ͽ�ƽ��־��" << (iter->second)->CombOffsetFlag << "  "
				<< "�۸�" << (iter->second)->LimitPrice << "  " << "������" << (iter->second)->VolumeTotalOriginal << "  "
				<< "�������ã�" << (iter->second)->OrderRef << "  " << "�ͻ����룺" << (iter->second)->ClientID << "  "
				<< "����״̬��" << (iter->second)->OrderStatus << "  " << "ί��ʱ�䣺" << (iter->second)->InsertTime << "  "
				<< "������ţ�" << (iter->second)->OrderStatus << "  " << "�����գ�" << (iter->second)->TradingDay << "  "
				<< "�������ڣ�" << (iter->second)->InsertDate << std::endl;

		}
		std::cerr << "---------------��ӡ�������---------------" << std::endl;
		std::cerr << std::endl << std::endl << std::endl;
	}
}

void TdSpi::Release()
{
	//���orderList����������б�������
	for (auto it = orderList.begin(); it != orderList.end(); it++)
	{
		delete it->second;
		it->second = nullptr;
	}
	orderList.clear();

	//��������гɽ�
	//std::vector<CThostFtdcTradeField*> tradeList;
	for (auto it = tradeList.begin(); it != tradeList.end(); it++)
	{
		delete (*it);
		*it = nullptr;
	}
	tradeList.clear();
}
