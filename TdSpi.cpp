#include "TdSpi.h"
#include <map>
#include <iostream>
#include <mutex>
#include <string>
#include "Strategy.h"

extern std::map<std::string, std::string> accountConfig_map;//保存账户信息的map
extern std::mutex m_mutex;
extern Strategy* g_strategy;//策略类指针

extern int g_nRequestID;

//全局持仓合约
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
	std::cerr << "当前的CTP API Version: " << version << std::endl;
	ReqAuthenticate();//进行穿透测试
}


int TdSpi::ReqAuthenticate()
{
	CThostFtdcReqAuthenticateField req;
	//初始化
	memset(&req, 0, sizeof(req));

	strcpy(req.AppID, m_AppId.c_str());
	//strcpy(req.AppID, "aaaaa");
	strcpy(req.AuthCode, m_AuthCode.c_str());
	//strcpy(req.AuthCode, "jjjjj");
	strcpy(req.BrokerID, m_BrokerId.c_str());
	//strcpy(req.BrokerID,"jjjjj");
	strcpy(req.UserID, m_UserId.c_str());
	//strcpy(req.UserID, "66666");
	std::cerr << "请求认证的账户信息: " << std::endl
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
			std::cerr << "穿透测试成功! " << "ErrMsg: " << pRspInfo->ErrorMsg << std::endl;
			ReqUserLogin();
		}
		else
		{
			std::cerr << "穿透测试失败! " << "Errorid: : " << pRspInfo->ErrorID << "ErrMsg: " << pRspInfo->ErrorMsg << std::endl;
		}
	}

}

int TdSpi::ReqUserLogin()
{
	std::cerr << "====ReqUserLogin====,用户登录中..." << std::endl;
	//定义一个CThostFtdcReqUserLoginField
	CThostFtdcReqUserLoginField reqUserLogin;
	//初始化为0
	memset(&reqUserLogin, 0, sizeof(reqUserLogin));
	//复制brokerid,userid,passwd
	strcpy(reqUserLogin.BrokerID, m_BrokerId.c_str());
	//strcpy(reqUserLogin.BrokerID,"0000");//BrokerID如果错误，报错errorid：3，不合法的ctp登录
	strcpy(reqUserLogin.UserID, m_UserId.c_str());
	//strcpy(reqUserLogin.UserID, "00000000");//UserID如果错误，报错errorid：3，不合法的ctp登录
	strcpy(reqUserLogin.Password, m_Passwd.c_str());
	//strcpy(reqUserLogin.Password, "000000");//UserID如果错误，报错errorid：3，不合法的ctp登录

	//登录
	return m_pUserTDApi_trade->ReqUserLogin(&reqUserLogin, GetNextRequestID());
}

bool TdSpi::IsErrorRspInfo(CThostFtdcRspInfoField* pRspInfo)
{
	// 如果ErrorID != 0, 说明收到了错误的响应
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	if (bResult)
		std::cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << std::endl;
	return bResult;
}

void TdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	std::cerr << "登录请求回调OnRspUserLogin" << std::endl;
	if (!IsErrorRspInfo(pRspInfo) && pRspUserLogin)
	{
		m_nFrontID = pRspUserLogin->FrontID;
		m_nSessionID = pRspUserLogin->SessionID;
		int nextOrderRef = atoi(pRspUserLogin->MaxOrderRef);

		sprintf_s(orderRef, sizeof(orderRef), "%d", ++nextOrderRef);

		std::cout << "前置编号:" << pRspUserLogin->FrontID << std::endl
			<< "会话编号" << pRspUserLogin->SessionID << std::endl
			<< "最大报单引用:" << pRspUserLogin->MaxOrderRef << std::endl
			<< "上期所时间：" << pRspUserLogin->SHFETime << std::endl
			<< "大商所时间：" << pRspUserLogin->DCETime << std::endl
			<< "郑商所时间：" << pRspUserLogin->CZCETime << std::endl
			<< "中金所时间：" << pRspUserLogin->FFEXTime << std::endl
			<< "能源所时间：" << pRspUserLogin->INETime << std::endl
			<< "交易日：" << m_pUserTDApi_trade->GetTradingDay() << std::endl;
		strcpy(m_cTradingDay, m_pUserTDApi_trade->GetTradingDay());//设置交易日期

		std::cout << "--------------------------------------------" << std::endl << std::endl;
	}

	ReqSettlementInfoConfirm();//请求确认结算单
}

void TdSpi::ReqSettlementInfoConfirm()
{
	CThostFtdcSettlementInfoConfirmField req;//定义
	memset(&req, 0, sizeof(req));//初始化
	strcpy(req.BrokerID, m_BrokerId.c_str());
	//strcpy(req.BrokerID, "0000");//BrokerID错误，结算单确认报错ErrorID=9, ErrorMsg=CTP:无此权限
	strcpy(req.InvestorID, m_UserId.c_str());
	//strcpy(req.InvestorID, "000000");//UserId错误，结算单确认报错ErrorID=9, ErrorMsg=CTP:无此权限
	int iResult = m_pUserTDApi_trade->ReqSettlementInfoConfirm(&req, GetNextRequestID());
	std::cerr << "--->>> 投资者结算结果确认: " << ((iResult == 0) ? "成功" : "失败") << std::endl;
}

void TdSpi::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField* pSettlementInfoConfirm, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	//bIsLast--是否是最后一条信息
	if (bIsLast && !IsErrorRspInfo(pRspInfo) && pSettlementInfoConfirm)
	{
		std::cerr << "响应 | 结算单..." << pSettlementInfoConfirm->InvestorID
			<< "..." << pSettlementInfoConfirm->ConfirmDate << "," <<
			pSettlementInfoConfirm->ConfirmTime << "...确认" << std::endl << std::endl;

		std::cerr << "结算单确认正常，首次查询报单" << std::endl;
		//线程休眠3秒，让ctp柜台有充足的响应时间，然后再进行查询操作
		std::chrono::milliseconds sleepDuration(3 * 1000);
		std::this_thread::sleep_for(sleepDuration);
		ReqQryOrder();//查询报单
	}
}

void TdSpi::ReqQryOrder()
{
	CThostFtdcQryOrderField  QryOrderField;//定义
	memset(&QryOrderField, 0, sizeof(CThostFtdcQryOrderField));//初始化为0
	//brokerid有误
	//strcpy(QryOrderField.BrokerID, "0000");
	strcpy(QryOrderField.BrokerID, m_BrokerId.c_str());
	//InvestorID有误
	//strcpy(QryOrderField.InvestorID, "666666");
	strcpy(QryOrderField.InvestorID, m_UserId.c_str());
	//调用api的ReqQryOrder
	m_pUserTDApi_trade->ReqQryOrder(&QryOrderField, GetNextRequestID());
}

void TdSpi::OnRspQryOrder(CThostFtdcOrderField* pOrder, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	std::cerr << "请求查询报单响应：OnRspQryOrder" << ",pOrder  " << pOrder << std::endl;
	if (!IsErrorRspInfo(pRspInfo) && pOrder)
	{
		std::cerr << "请求查询报单响应：前置编号FrontID：" << pOrder->FrontID << ",会话编号:" << pOrder->SessionID
			<< ",报单引用:  " << pOrder->OrderRef << std::endl;
		//所有合约
		if (m_QryOrder_Once == true)
		{
			CThostFtdcOrderField* order = new CThostFtdcOrderField();
			memcpy(order, pOrder, sizeof(CThostFtdcOrderField));
			orderList.insert(std::make_pair(pOrder->BrokerOrderSeq, order));



			//bIsLast是否是最后一笔回报
			if (bIsLast)
			{
				m_QryOrder_Once = false;
				std::cerr << "所有合约的报单次数" << orderList.size() << std::endl;
				std::cerr << "---------------打印报单开始---------------" << std::endl;
				for (auto iter = orderList.begin(); iter != orderList.end(); iter++)
				{
					std::cerr << "经纪公司代码：" << (iter->second)->BrokerID << "  " << "投资者代码：" << (iter->second)->InvestorID << "  "
						<< "用户代码：" << (iter->second)->UserID << "  " << "合约代码：" << (iter->second)->InstrumentID << "  "
						<< "买卖方向：" << (iter->second)->Direction << "  " << "组合开平标志：" << (iter->second)->CombOffsetFlag << "  "
						<< "价格：" << (iter->second)->LimitPrice << "  " << "数量：" << (iter->second)->VolumeTotalOriginal << "  "
						<< "报单引用：" << (iter->second)->OrderRef << "  " << "客户代码：" << (iter->second)->ClientID << "  "
						<< "报单状态：" << (iter->second)->OrderStatus << "  " << "委托时间：" << (iter->second)->InsertTime << "  "
						<< "报单编号：" << (iter->second)->OrderStatus << "  " << "交易日：" << (iter->second)->TradingDay << "  "
						<< "报单日期：" << (iter->second)->InsertDate << std::endl;

				}
				std::cerr << "---------------打印报单完成---------------" << std::endl;
				std::cerr << "查询报单正常，将首次查询成交" << std::endl;


			}
		}

	}
	else//查询出错
	{
		m_QryOrder_Once = false;
		std::cerr << "查询报单出错，或没有成交，将首次查询成交" << std::endl;

	}
	if (bIsLast)
	{
		//线程休眠3秒，让ctp柜台有充足的响应时间，然后再进行查询操作
		std::chrono::milliseconds sleepDuration(3 * 1000);
		std::this_thread::sleep_for(sleepDuration);
		ReqQryTrade();
	}
}

void TdSpi::ReqQryTrade()
{
	CThostFtdcQryTradeField tdField;//定义
	memset(&tdField, 0, sizeof(tdField));//初始化

	strcpy(tdField.BrokerID, m_BrokerId.c_str());
	//strcpy(tdField.BrokerID,"0000");
	strcpy(tdField.InvestorID, m_UserId.c_str());
	//strcpy(tdField.InvestorID, "888888");
	//调用交易api的ReqQryTrade
	m_pUserTDApi_trade->ReqQryTrade(&tdField, GetNextRequestID());
}

void TdSpi::OnRspQryTrade(CThostFtdcTradeField* pTrade, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{

	std::cerr << "请求查询成交回报响应：OnRspQryTrade" << " pTrade " << pTrade << std::endl;

	if (!IsErrorRspInfo(pRspInfo) && pTrade)
	{
		//所有合约
		if (m_QryTrade_Once == true)
		{
			CThostFtdcTradeField* trade = new CThostFtdcTradeField();//创建trade结构体
			memcpy(trade, pTrade, sizeof(CThostFtdcTradeField));//pTrade复制给trade
			tradeList.push_back(trade);

			if (bIsLast)
			{
				m_QryTrade_Once = false;
				std::cerr << "所有合约的成交次数" << tradeList.size() << std::endl;
				std::cerr << "---------------打印成交开始---------------" << std::endl;
				for (std::vector<CThostFtdcTradeField*>::iterator iter = tradeList.begin(); iter != tradeList.end(); iter++)
				{
					std::cerr << "经纪公司代码：" << (*iter)->BrokerID << std::endl << "投资者代码：" << (*iter)->InvestorID << std::endl
						<< "用户代码：" << (*iter)->UserID << std::endl << "成交编号：" << (*iter)->TradeID << std::endl
						<< "合约代码：" << (*iter)->InstrumentID << std::endl << "买卖方向：" << (*iter)->Direction << std::endl
						<< "组合开平标志：" << (*iter)->OffsetFlag << std::endl << "投机套保标志：" << (*iter)->HedgeFlag << std::endl
						<< "价格：" << (*iter)->Price << std::endl << "数量：" << (*iter)->Volume << std::endl
						<< "报单引用：" << (*iter)->OrderRef << std::endl << "本地报单编号：" << (*iter)->OrderLocalID << std::endl
						<< "成交时间：" << (*iter)->TradeTime << std::endl << "业务单元：" << (*iter)->BusinessUnit << std::endl
						<< "序号：" << (*iter)->SequenceNo << std::endl << "经纪公司下单序号：" << (*iter)->BrokerOrderSeq << std::endl
						<< "交易日：" << (*iter)->TradingDay << std::endl;
					std::cerr << "---------单笔成交结束--------" << std::endl;
				}
				std::cerr << "---------------打印成交完成---------------" << std::endl;
				std::cerr << "查询报单正常，将首次查询持仓明细" << std::endl;
			}
		}
	}
	else//查询出错
	{
		m_QryOrder_Once = false;
		std::cerr << "查询报单出错，或没有成交，将首次查询成交" << std::endl;

	}
	if (bIsLast)
	{
		//线程休眠3秒，让ctp柜台有充足的响应时间，然后再进行查询操作
		std::chrono::milliseconds sleepDuration(3 * 1000);
		std::this_thread::sleep_for(sleepDuration);
		ReqQryInvestorPositionDetail();//查询持仓明细
	}
}

void TdSpi::ReqQryInvestorPositionDetail()
{
	CThostFtdcQryInvestorPositionDetailField pdField;//创建
	memset(&pdField, 0, sizeof(pdField));//初始化为0
	strcpy(pdField.BrokerID, m_BrokerId.c_str());
	//strcpy(pdField.BrokerID, "0000");
	//strcpy(pdField.InstrumentID, m_InstId.c_str());

	strcpy(pdField.InvestorID, m_UserId.c_str());

	//strcpy(pdField.InvestorID, "0000");
	//调用交易api的ReqQryInvestorPositionDetail
	m_pUserTDApi_trade->ReqQryInvestorPositionDetail(&pdField, GetNextRequestID());
}

void TdSpi::OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField* pField, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	std::cerr << "请求查询投资者持仓明细回报响应：OnRspQryInvestorPositionDetail" << " pInvestorPositionDetail " << pField << std::endl;
	if (!IsErrorRspInfo(pRspInfo) && pField)
	{
		//所有合约
		if (m_QryDetail_Once == true)//仅在程序开始执行一次
		{
			//对于所有合约，只保存未平仓的，不保存已经平仓的
			//将程序启动前的持仓记录保存到未平仓容器tradeList_NotClosed_Long和tradeList_NotClosed_Short
			//使用结构体CThostFtdcTradeField，因为它有时间字段，而CThostFtdcInvestorPositionDetailField没有时间字段
			CThostFtdcTradeField* trade = new CThostFtdcTradeField();//创建CThostFtdcTradeField *

			strcpy(trade->InvestorID, pField->InvestorID);///投资者代码
			strcpy(trade->InstrumentID, pField->InstrumentID);///合约代码
			strcpy(trade->ExchangeID, pField->ExchangeID);///交易所代码
			trade->Direction = pField->Direction;//买卖方向
			trade->Price = pField->OpenPrice;//价格
			trade->Volume = pField->Volume;//数量
			strcpy(trade->TradeDate, pField->OpenDate);//成交日期
			strcpy(trade->TradeID, pField->TradeID);//*********成交编号********
			if (pField->Volume > 0)//筛选未平仓合约
			{
				if (trade->Direction == '0')//买入方向
					positionDetailList_NotClosed_Long.push_back(trade);
				else if (trade->Direction == '1')//卖出方向
					positionDetailList_NotClosed_Short.push_back(trade);
			}
			//收集持仓合约的代码
			bool find_instId = false;
			for (unsigned int i = 0; i < subscribe_inst_vec.size(); i++)
			{
				if (strcmp(subscribe_inst_vec[i].c_str(), trade->InstrumentID) == 0)//合约已存在，已订阅，相等则返回0
				/*strcmp函数是string compare(字符串比较)的缩写，用于比较两个字符串并根据比较结果返回整数。基本形式为strcmp(str1,str2)，若str1=str2，则返回零；若str1<str2，则返回负数；若str1>str2，则返回正数。*/
				{
					find_instId = true;
					break;
				}
			}
			if (!find_instId)//合约未订阅过
			{
				std::cerr << trade->InstrumentID <<" :该持仓合约未订阅过，加入订阅列表" << std::endl;
				subscribe_inst_vec.push_back(trade->InstrumentID);
			}

		}
		//输出所有合约的持仓明细，要在这边进行下一步的查询ReqQryTradingAccount()
		if (bIsLast)
		{
			m_QryDetail_Once = false;
			//持仓的合约
			std::string inst_holding;
			//
			for (unsigned int i = 0; i < subscribe_inst_vec.size(); i++)
				inst_holding = inst_holding + subscribe_inst_vec[i] + ",";
			//"IF2102,IF2103,"

			inst_holding = inst_holding.substr(0, inst_holding.length() - 1);//去掉最后的逗号，从位置0开始，选取length-1个字符
			//"IF2102,IF2103"

			std::cerr << "程序启动前的持仓列表:" << inst_holding << ",inst_holding.length()=" << inst_holding.length()
				<< ",subscribe_inst_vec.size()=" << subscribe_inst_vec.size() << std::endl;

			if (inst_holding.length() > 0)
				m_pUserMDSpi_trade->setInstIdList_Position_MD(inst_holding);//设置程序启动前的留仓，即需要订阅行情的合约

			//size代表笔数，而不是手数
			std::cerr << "账户所有合约未平仓单笔数（下单笔数，一笔可以对应多手）,多单:" << positionDetailList_NotClosed_Long.size()
				<< "空单：" << positionDetailList_NotClosed_Short.size() << std::endl;


			std::cerr << "-----------------------------------------未平仓多单明细打印start" << std::endl;
			for (std::vector<CThostFtdcTradeField*>::iterator iter = positionDetailList_NotClosed_Long.begin(); iter != positionDetailList_NotClosed_Long.end(); iter++)
			{
				std::cerr << "BrokerID:" << (*iter)->BrokerID << std::endl << "InvestorID:" << (*iter)->InvestorID << std::endl
					<< "InstrumentID:" << (*iter)->InstrumentID << std::endl << "Direction:" << (*iter)->Direction << std::endl
					<< "OpenPrice:" << (*iter)->Price << std::endl << "Volume:" << (*iter)->Volume << std::endl
					<< "TradeDate:" << (*iter)->TradeDate << std::endl << "TradeID:" << (*iter)->TradeID << std::endl;
			}

			std::cerr << "-----------------------------------------未平仓空单明细打印start" << std::endl;
			for (std::vector<CThostFtdcTradeField*>::iterator iter = positionDetailList_NotClosed_Short.begin(); iter != positionDetailList_NotClosed_Short.end(); iter++)
			{
				std::cerr << "BrokerID:" << (*iter)->BrokerID << std::endl << "InvestorID:" << (*iter)->InvestorID << std::endl
					<< "InstrumentID:" << (*iter)->InstrumentID << std::endl << "Direction:" << (*iter)->Direction << std::endl
					<< "OpenPrice:" << (*iter)->Price << std::endl << "Volume:" << (*iter)->Volume << std::endl
					<< "TradeDate:" << (*iter)->TradeDate << std::endl << "TradeID:" << (*iter)->TradeID << std::endl;
			}
			std::cerr << "---------------打印持仓明细完成---------------" << std::endl;
			std::cerr << "查询持仓明细正常，将首次查询账户资金信息" << std::endl;
		}

	}
	else
	{
		if (m_QryDetail_Once == true)
		{
			m_QryDetail_Once = false;
			std::cerr << "查询持仓明细出错，或没有持仓明细，将首次查询账户资金" << std::endl;
		}
	}
	if (bIsLast)
	{
		//线程休眠3秒，让ctp柜台有充足的响应时间，然后再进行查询操作
		std::chrono::milliseconds sleepDuration(3 * 1000);
		std::this_thread::sleep_for(sleepDuration);
		ReqQryTradingAccount();//请求查询资金账户
	}
}

void TdSpi::ReqQryTradingAccount()//请求查询资金账户
{
	CThostFtdcQryTradingAccountField req;//创建req的结构体对象
	memset(&req, 0, sizeof(req));//初始化
	//错误的brokerID
	//strcpy(req.BrokerID, "8888");
	strcpy(req.BrokerID, m_BrokerId.c_str());
	strcpy(req.InvestorID, m_UserId.c_str());
	//strcpy(req.InvestorID, "666666");
	//调用交易api的ReqQryTradingAccount
	int iResult = m_pUserTDApi_trade->ReqQryTradingAccount(&req, GetNextRequestID());
	std::cerr << "--->>> 请求查询资金账户: " << ((iResult == 0) ? "成功" : "失败") << std::endl;
}

//查询资金账户响应
void TdSpi::OnRspQryTradingAccount(CThostFtdcTradingAccountField* pTradingAccount, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	std::cerr << "请求查询投资者资金账户回报响应：OnRspQryTradingAccount" << " pTradingAccount " << pTradingAccount << std::endl;
	if (!IsErrorRspInfo(pRspInfo) && pTradingAccount)
	{

		std::cerr << "投资者编号：" << pTradingAccount->AccountID
			<< ", 静态权益：期初权益" << pTradingAccount->PreBalance
			<< ", 动态权益：期货结算准备金" << pTradingAccount->Balance
			<< ", 可用资金：" << pTradingAccount->Available
			<< ", 可取资金：" << pTradingAccount->WithdrawQuota
			<< ", 当前保证金总额：" << pTradingAccount->CurrMargin
			<< ", 平仓盈亏：" << pTradingAccount->CloseProfit
			<< ", 持仓盈亏：" << pTradingAccount->PositionProfit
			<< ", 手续费：" << pTradingAccount->Commission
			<< ", 冻结保证金：" << pTradingAccount->FrozenCash
			<< std::endl;
		//所有合约
		if (m_QryTradingAccount_Once == true)
		{
			m_QryTradingAccount_Once = false;
		}

		std::cerr << "---------------打印资金账户明细完成---------------" << std::endl;
		std::cerr << "查询资金账户正常，将首次查询投资者持仓信息" << std::endl;
	}
	else
	{
		if (m_QryTradingAccount_Once == true)//仅程序开始查询一次
		{
			m_QryTradingAccount_Once = false;
			std::cerr << "查询资金账户出错，将首次查询投资者持仓" << std::endl;
		}
	}
	//线程休眠3秒，让ctp柜台有充足的响应时间，然后再进行查询操作
	std::chrono::milliseconds sleepDuration(3 * 1000);
	std::this_thread::sleep_for(sleepDuration);
	ReqQryInvestorPosition_All();//查询所有合约的持仓
}

void TdSpi::ReqQryInvestorPosition_All()//查询所有合约的持仓
{
	CThostFtdcQryInvestorPositionField req;//创建req
	memset(&req, 0, sizeof(req));//初始化为0

	//strcpy(req.BrokerID, "8888");
	//strcpy(req.BrokerID, m_BrokerId.c_str());
	//strcpy(req.InvestorID, m_UserId.c_str());
	//strcpy(req.InvestorID, "0000");
	//合约为空，则代表查询所有合约的持仓，这个和req为空是一样的
	//strcpy(req.InstrumentID, m_InstId.c_str());
	//调用交易api的ReqQryInvestorPosition
	int iResult = m_pUserTDApi_trade->ReqQryInvestorPosition(&req, GetNextRequestID());//如果req为空，代表查询所有合约的持仓，如果写了具体的某个合约，则查询那个合约。
	std::cerr << "--->>> 请求查询投资者持仓: " << ((iResult == 0) ? "成功" : "失败") << std::endl;
}

//查询所有合约的持仓响应
void TdSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField* pInvestorPosition,
	CThostFtdcRspInfoField* pRspInfo, int g_nRequestID, bool bIsLast) {
	//cerr << "请求查询持仓响应：OnRspQryInvestorPosition " << ",pInvestorPosition  " << pInvestorPosition << endl;
	if (!IsErrorRspInfo(pRspInfo) && pInvestorPosition)
	{

		//账户下所有合约
		if (m_QryPosition_Once == true)
		{
			std::cerr << "请求查询持仓响应：OnRspQryInvestorPosition " << " pInvestorPosition:  "
				<< pInvestorPosition << std::endl;//会包括已经平仓没有持仓的记录
			std::cerr << "响应  | 合约 " << pInvestorPosition->InstrumentID << std::endl
				<< " 持仓多空方向 " << pInvestorPosition->PosiDirection << std::endl//2多3空
				// << " 映射后的方向 " << MapDirection(pInvestorPosition->PosiDirection-2,false) << endl
				<< " 总持仓 " << pInvestorPosition->Position << std::endl
				<< " 今日持仓 " << pInvestorPosition->TodayPosition << std::endl
				<< " 上日持仓 " << pInvestorPosition->YdPosition << std::endl
				<< " 保证金 " << pInvestorPosition->UseMargin << std::endl
				<< " 持仓成本 " << pInvestorPosition->PositionCost << std::endl
				<< " 开仓量 " << pInvestorPosition->OpenVolume << std::endl
				<< " 平仓量 " << pInvestorPosition->CloseVolume << std::endl
				<< " 持仓日期 " << pInvestorPosition->TradingDay << std::endl
				<< " 平仓盈亏（按昨结） " << pInvestorPosition->CloseProfitByDate << std::endl
				<< " 持仓盈亏 " << pInvestorPosition->PositionProfit << std::endl
				<< " 逐日盯市平仓盈亏（按昨结） " << pInvestorPosition->CloseProfitByDate << std::endl//快期中显示的是这个值
				<< " 逐笔对冲平仓盈亏（按开平合约） " << pInvestorPosition->CloseProfitByTrade << std::endl//在交易中比较有意义
				<< std::endl;


			//构造合约对应持仓明细信息的结构体map
			bool  find_trade_message_map = false;
			for (std::map<std::string, position_field*>::iterator iter = m_position_field_map.begin(); iter != m_position_field_map.end(); iter++)
			{
				if (strcmp((iter->first).c_str(), pInvestorPosition->InstrumentID) == 0)//合约已存在
				{
					find_trade_message_map = true;
					break;
				}
			}
			if (!find_trade_message_map)//合约不存在
			{
				std::cerr << "-----------------------没有这个合约，需要构造交易信息结构体" << std::endl;
				position_field* p_trade_message = new position_field();
				p_trade_message->instId = pInvestorPosition->InstrumentID;
				m_position_field_map.insert(std::pair<std::string, position_field*>(pInvestorPosition->InstrumentID, p_trade_message));
			}
			if (pInvestorPosition->PosiDirection == '2')//多单
			{
				//昨仓和今仓一次返回
				//获取该合约的持仓明细信息结构体 second; m_map[键]
				position_field* p_tdm = m_position_field_map[pInvestorPosition->InstrumentID];
				p_tdm->LongPosition = p_tdm->LongPosition + pInvestorPosition->Position;
				p_tdm->TodayLongPosition = p_tdm->TodayLongPosition + pInvestorPosition->TodayPosition;
				p_tdm->YdLongPosition = p_tdm->LongPosition - p_tdm->TodayLongPosition;
				p_tdm->LongCloseProfit = p_tdm->LongCloseProfit + pInvestorPosition->CloseProfit;
				p_tdm->LongPositionProfit = p_tdm->LongPositionProfit + pInvestorPosition->PositionProfit;
			}
			else if (pInvestorPosition->PosiDirection == '3')//空单
			{
				//昨仓和今仓一次返回

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
					std::cerr << "合约代码：" << iter->second->instId << std::endl
						<< "多单持仓量：" << iter->second->LongPosition << std::endl
						<< "空单持仓量：" << iter->second->ShortPosition << std::endl
						<< "多单今日持仓：" << iter->second->TodayLongPosition << std::endl
						<< "多单上日持仓：" << iter->second->YdLongPosition << std::endl
						<< "空单今日持仓：" << iter->second->TodayShortPosition << std::endl
						<< "空单上日持仓：" << iter->second->YdShortPosition << std::endl
						<< "多单浮动盈亏：" << iter->second->LongPositionProfit << std::endl
						<< "多单平仓盈亏：" << iter->second->LongCloseProfit << std::endl
						<< "空单浮动盈亏：" << iter->second->ShortPositionProfit << std::endl
						<< "空单平仓盈亏：" << iter->second->ShortCloseProfit << std::endl;

					//账户平仓盈亏
					m_CloseProfit = m_CloseProfit + iter->second->LongCloseProfit + iter->second->ShortCloseProfit;
					//账户浮动盈亏
					m_OpenProfit = m_OpenProfit + iter->second->LongPositionProfit + iter->second->ShortPositionProfit;
				}
				std::cerr << "------------------------" << std::endl;
				std::cerr << "账户浮动盈亏 " << m_OpenProfit << std::endl;
				std::cerr << "账户平仓盈亏 " << m_CloseProfit << std::endl;
			}//bisLast


		}
		std::cerr << "---------------查询投资者持仓完成---------------" << std::endl;
		std::cerr << "查询持仓正常，首次查询所有合约代码" << std::endl;
	}
	else
	{
		if (m_QryPosition_Once == true)
			m_QryPosition_Once = false;
		std::cerr << "查询投资者持仓出错，或没有持仓，首次查询所有合约" << std::endl;
	}
	if (bIsLast)
	{
		//线程休眠3秒，让ctp柜台有充足的响应时间，然后再进行查询操作
		std::chrono::milliseconds sleepDuration(10 * 1000);
		std::this_thread::sleep_for(sleepDuration);
		ReqQryInstrumetAll();
	}

}
// 查询单个期货合约
void TdSpi::ReqQryInvestorPosition(char* pInstrument)
{
	CThostFtdcQryInvestorPositionField req;//创建req
	memset(&req, 0, sizeof(req));//初始化为0

	 
	strcpy(req.BrokerID, m_BrokerId.c_str());
	strcpy(req.InvestorID, m_UserId.c_str());

	//合约填写具体的合约代码
	strcpy(req.InstrumentID, pInstrument);
	//调用交易api的ReqQryInvestorPosition
	int iResult = m_pUserTDApi_trade->ReqQryInvestorPosition(&req, GetNextRequestID());//req为空，代表查询所有合约的持仓
	std::cerr << "--->>> 请求查询投资者持仓: " << ((iResult == 0) ? "成功" : "失败") << std::endl;
}

void TdSpi::ReqQryInstrumetAll()//查询所有合约
{
	CThostFtdcQryInstrumentField req;//创建req
	memset(&req, 0, sizeof(req));//初始化为0

	//调用交易api的ReqQryInstrument
	int iResult = m_pUserTDApi_trade->ReqQryInstrument(&req, GetNextRequestID());//req结构体为0，查询所有合约
	std::cerr << "--->>> 请求查询合约: " << ((iResult == 0) ? "成功" : "失败") << std::endl;
}

void TdSpi::ReqQryInstrumet(char* pInstrument)//查询单个合约
{
	CThostFtdcQryInstrumentField req;//创建req
	memset(&req, 0, sizeof(req));//初始化为0
	strcpy(req.InstrumentID, pInstrument);//合约填写具体的代码，表示查询该合约的信息
	//调用交易api的ReqQryInstrument
	int iResult = m_pUserTDApi_trade->ReqQryInstrument(&req, GetNextRequestID());//
	std::cerr << "--->>> 请求查询合约: " << ((iResult == 0) ? "成功" : "失败") << std::endl;
}

//查询合约响应
void TdSpi::OnRspQryInstrument(CThostFtdcInstrumentField* pInstrument, CThostFtdcRspInfoField* pRspInfo, int m_nNextRequestID, bool bIsLast)
{
	//cerr << "请求查询合约响应：OnRspQryInstrument" << ",pInstrument   " << pInstrument->InstrumentID << endl;
	if (!IsErrorRspInfo(pRspInfo) && pInstrument)
	{

		//账户下所有合约
		if (m_QryInstrument_Once == true)
		{
			m_Instrument_All = m_Instrument_All + pInstrument->InstrumentID + ",";

			//保存所有合约信息到map
			CThostFtdcInstrumentField* pInstField = new CThostFtdcInstrumentField();
			memcpy(pInstField, pInstrument, sizeof(CThostFtdcInstrumentField));
			m_inst_field_map.insert(std::pair<std::string, CThostFtdcInstrumentField*>(pInstrument->InstrumentID, pInstField));

			//策略交易的合约
			if (strcmp(m_InstId.c_str(), pInstrument->InstrumentID) == 0)
			{
				std::cerr << "------------" << std::endl;
				std::cerr << "响应 | 合约：" << pInstrument->InstrumentID
					<< " ,合约名称：" << pInstrument->InstrumentName
					<< " ,合约在交易所代码：" << pInstrument->ExchangeInstID
					<< " ,产品代码：" << pInstrument->ProductID
					<< " ,产品类型：" << pInstrument->ProductClass
					<< " ,多头保证金率：" << pInstrument->LongMarginRatio
					<< " ,空头保证金率：" << pInstrument->ShortMarginRatio
					<< " ,合约数量乘数：" << pInstrument->VolumeMultiple
					<< " ,最小变动价位：" << pInstrument->PriceTick
					<< " ,交易所代码：" << pInstrument->ExchangeID
					<< " ,交割年份：" << pInstrument->DeliveryYear
					<< " ,交割月：" << pInstrument->DeliveryMonth
					<< " ,创建日：" << pInstrument->CreateDate
					<< " ,到期日：" << pInstrument->ExpireDate
					<< " ,上市日：" << pInstrument->OpenDate
					<< " ,开始交割日：" << pInstrument->StartDelivDate
					<< " ,结束交割日：" << pInstrument->EndDelivDate
					<< " ,合约生命周期状态：" << pInstrument->InstLifePhase
					<< " ,当前是否交易：" << pInstrument->IsTrading << std::endl;
				std::cerr<<"------------"<< std::endl;
			}

			if (bIsLast)
			{
				m_QryInstrument_Once = false;
				m_Instrument_All = m_Instrument_All.substr(0, m_Instrument_All.length() - 1);
				std::cerr << "m_Instrument_All的大小：" << m_Instrument_All.size() << std::endl;
				std::cerr << "map的大小（合约数量）：" << m_inst_field_map.size() << std::endl;

				//将持仓合约信息设置到mdspi
				//m_pUserMDSpi_trade->setInstIdList_Position_MD(m_Inst_Postion);


				//将合约信息结构体的map复制到策略类
				g_strategy->set_instPostion_map_stgy(m_inst_field_map);
				std::cerr << "--------------------------输出合约信息map的内容-----------------------" << std::endl;
				//ShowInstMessage();//启用打印全部合约信息,开启就会打印全市场合约信息
				//保存全市场合约，在TD进行，需要订阅全市场合约行情时再运行
				m_pUserMDSpi_trade->set_InstIdList_All(m_Instrument_All);
				std::cerr << "TD初始化完成，启动MD" << std::endl;
				m_pUserMDApi_trade->Init();//在使用init后，会到MdSpi里面，进行行情前置连接"MdSpi::OnFrontConnected()"
			}
		}
	}
	else
	{
		m_QryInstrument_Once = false;
		std::cerr << "查询合约失败" << std::endl;
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
	//1、首先保存并更新这个报单
	UpdateOrder(pOrder, orderList);


	//2、打印所有的报单

	ShowOrderList(orderList);


	////判断是否本程序发出的报单；

	////if (pOrder->FrontID != m_nFrontID || pOrder->SessionID != m_nSessionID) {

	////	CThostFtdcOrderField* pOld = GetOrder(pOrder->BrokerOrderSeq);
	////	if (pOld == NULL) {
	////		return;
	////	}
	////}

	char* pszStatus = new char[13];
	switch (pOrder->OrderStatus) {
	case THOST_FTDC_OST_AllTraded:
		strcpy(pszStatus, "全部成交");
		break;
	case THOST_FTDC_OST_PartTradedQueueing:
		strcpy(pszStatus, "部分成交");
		break;
	case THOST_FTDC_OST_NoTradeQueueing:
		strcpy(pszStatus, "未成交");
		break;
	case THOST_FTDC_OST_Canceled:
		strcpy(pszStatus, "已撤单");
		break;
	case THOST_FTDC_OST_Unknown:
		strcpy(pszStatus, "未知");
		break;
	case THOST_FTDC_OST_NotTouched:
		strcpy(pszStatus, "未触发");
		break;
	case THOST_FTDC_OST_Touched:

		strcpy(pszStatus, "已触发");
		break;
	default:
		strcpy(pszStatus, "");
		break;
	}


	///*printf("order returned,ins: %s, vol: %d, price:%f, orderref:%s,requestid:%d,traded vol: %d,ExchID:%s, OrderSysID:%s,status: %s,statusmsg:%s\n"
	//	, pOrder->InstrumentID, pOrder->VolumeTotalOriginal, pOrder->LimitPrice, pOrder->OrderRef, pOrder->RequestID
	//	, pOrder->VolumeTraded, pOrder->ExchangeID, pOrder->OrderSysID, pszStatus, pOrder->StatusMsg);*/


	////保存并更新报单的状态
	//UpdateOrder(pOrder);
	std::cerr << "BrokerOrderSeq:" << pOrder->BrokerOrderSeq << "  ,OrderRef;" << pOrder->OrderRef << " ,报单状态  " << pszStatus << ",开平标志  " << pOrder->CombOffsetFlag[0] << std::endl;

	//if (pOrder->OrderStatus == '3' || pOrder->OrderStatus == '1')
	//{
	//	CThostFtdcOrderField* pOld = GetOrder(pOrder->BrokerOrderSeq);
	//	if (pOld && pOld->OrderStatus != '6')
	//	{
	//		std::cerr << "onRtnOrder 准备撤单了:" <<std::endl;
	//		CancelOrder(pOrder);
	//	}
	//}



}

void TdSpi::OnRtnTrade(CThostFtdcTradeField* pTrade)
{
	//	///开仓
//#define THOST_FTDC_OF_Open '0'
/////平仓
//#define THOST_FTDC_OF_Close '1'
/////强平
//#define THOST_FTDC_OF_ForceClose '2'
/////平今
//#define THOST_FTDC_OF_CloseToday '3'
/////平昨
//#define THOST_FTDC_OF_CloseYesterday '4'
/////强减
//#define THOST_FTDC_OF_ForceOff '5'
/////本地强平
//#define THOST_FTDC_OF_LocalForceClose '6'
//
//	typedef char TThostFtdcOffsetFlagType;


		//printf("trade returned,ins: %s, trade vol: %d, trade price:%f, ExchID:%s, OrderSysID:%s,开平:%c\n"
		//	, pTrade->InstrumentID, pTrade->Volume, pTrade->Price, pTrade->ExchangeID, pTrade->OrderSysID, pTrade->OffsetFlag);
	

	//需要判断是否是断线重连
	//1、编写一个FindTrade函数
	bool bFind = false;

	bFind = FindTrade(pTrade);

	//2、如果没有记录，则插入成交数据，编写一个插入成交的函数
	if (!bFind)
	{
		InsertTrade(pTrade);
		ShowTradeList();
	}
	else//bFind为true，找到了
		return;



	////判断是否断线重传；
	////如果程序断线重连以后，成交会再次刷新一次
	//std::set<std::string>::iterator it = m_TradeIDs.find(pTrade->TradeID);
	////成交已存在
	//if (it != m_TradeIDs.end()) 
	//{
	//	return;
	//}
	////成交id它不在我们的set集合里面

	////判断是否本程序发出的报单；
	//CThostFtdcOrderField* pOrder = GetOrder(pTrade->BrokerOrderSeq);
	//if (pOrder != NULL) 
	//{
	//	//只处理本程序发出的报单；

	//	//插入成交到set
	//	{
	//		std::lock_guard<std::mutex> m_lock(m_mutex);//加锁，保证这个set数据的安全
	//		m_TradeIDs.insert(pOrder->TraderID);
	//	}

	//	printf("trade returned,ins: %s, trade vol: %d, trade price:%f, ExchID:%s, OrderSysID:%s\n"
	//		, pTrade->InstrumentID, pTrade->Volume, pTrade->Price, pTrade->ExchangeID, pTrade->OrderSysID);

	//	//g_strategy->OnRtnTrade(pTrade);

	//}
}

void TdSpi::PlaceOrder(const char* pszCode, const char* pszExchangeID, int nDirection, int nOpenClose, int nVolume, double fPrice)
{
	//创建报单结构体对象
	CThostFtdcInputOrderField orderField;
	//初始化清零
	memset(&orderField, 0, sizeof(CThostFtdcInputOrderField));

	//fill the broker and user fields;

	strcpy(orderField.BrokerID, m_BrokerId.c_str());//复制我们的brokerid到结构体对象
	strcpy(orderField.InvestorID, m_UserId.c_str());//复制我们的InvestorID到结构体对象

	//set the Symbol code;
	strcpy(orderField.InstrumentID, pszCode);//设置下单的期货合约
	CThostFtdcInstrumentField* instField = m_inst_field_map.find(pszCode)->second;
	/*if (instField)
	{
		const char* ExID = instField->ExchangeID;
		strcpy(orderField.ExchangeID, ExID);
	}*/
	strcpy(orderField.ExchangeID, pszExchangeID);//交易所代码，"SHFE","CFFEX","DCE","CZCE","INE"
	if (nDirection == 0) {
		orderField.Direction = THOST_FTDC_D_Buy;
	}
	else {
		orderField.Direction = THOST_FTDC_D_Sell;
	}

	orderField.LimitPrice = fPrice;//价格

	orderField.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;//投机还是套利，套保，做市等



	orderField.VolumeTotalOriginal = nVolume;//下单手数

	//--------------1、价格条件----------------
		//限价单；
	orderField.OrderPriceType = THOST_FTDC_OPT_LimitPrice;//报单的价格类型条件

	//市价单；
	//orderField.OrderPriceType=THOST_FTDC_OPT_AnyPrice;
	//中金所市价
	//orderField.OrderPriceType = THOST_FTDC_OPT_FiveLevelPrice;

	//--------------2、触发条件----------------
	//	///立即
//#define THOST_FTDC_CC_Immediately '1'
/////止损
//#define THOST_FTDC_CC_Touch '2'
/////止赢
//#define THOST_FTDC_CC_TouchProfit '3'
/////预埋单
//#define THOST_FTDC_CC_ParkedOrder '4'
/////最新价大于条件价
//#define THOST_FTDC_CC_LastPriceGreaterThanStopPrice '5'
/////最新价大于等于条件价
//#define THOST_FTDC_CC_LastPriceGreaterEqualStopPrice '6'
/////最新价小于条件价
//#define THOST_FTDC_CC_LastPriceLesserThanStopPrice '7'
/////最新价小于等于条件价
//#define THOST_FTDC_CC_LastPriceLesserEqualStopPrice '8'
/////卖一价大于条件价
//#define THOST_FTDC_CC_AskPriceGreaterThanStopPrice '9'
/////卖一价大于等于条件价
//#define THOST_FTDC_CC_AskPriceGreaterEqualStopPrice 'A'
/////卖一价小于条件价
//#define THOST_FTDC_CC_AskPriceLesserThanStopPrice 'B'
/////卖一价小于等于条件价
//#define THOST_FTDC_CC_AskPriceLesserEqualStopPrice 'C'
/////买一价大于条件价
//#define THOST_FTDC_CC_BidPriceGreaterThanStopPrice 'D'
/////买一价大于等于条件价
//#define THOST_FTDC_CC_BidPriceGreaterEqualStopPrice 'E'
/////买一价小于条件价
//#define THOST_FTDC_CC_BidPriceLesserThanStopPrice 'F'
/////买一价小于等于条件价
//#define THOST_FTDC_CC_BidPriceLesserEqualStopPrice 'H'

	orderField.ContingentCondition = THOST_FTDC_CC_Immediately;//报单的触发条件
	//orderField.ContingentCondition = THOST_FTDC_CC_LastPriceGreaterThanStopPrice;//报单的触发条件
	//orderField.StopPrice = 5035.0;//



	//--------------3、时间条件----------------
	//orderField.TimeCondition = THOST_FTDC_TC_IOC;

//	///立即完成，否则撤销
//#define THOST_FTDC_TC_IOC '1'
/////本节有效
//#define THOST_FTDC_TC_GFS '2'
/////当日有效
//#define THOST_FTDC_TC_GFD '3'
/////指定日期前有效
//#define THOST_FTDC_TC_GTD '4'
/////撤销前有效
//#define THOST_FTDC_TC_GTC '5'
/////集合竞价有效
//#define THOST_FTDC_TC_GFA '6'


	//orderField.TimeCondition = THOST_FTDC_TC_GFS;//时间条件,本节有效,------没法用，错单，上期所、中金所：不被支持的报单类型
	//orderField.TimeCondition = THOST_FTDC_TC_GTD;//时间条件,指定日期有效,错单，不被支持的报单类型
	//orderField.TimeCondition = THOST_FTDC_TC_GTC;//时间条件,撤销前有效,错单，不被支持的报单类型
	//orderField.TimeCondition = THOST_FTDC_TC_GFA;//时间条件,集合竞价有效,错单，不被支持的报单类型


	//orderField.TimeCondition = THOST_FTDC_TC_IOC;//时间条件，立即完成，否则撤销
	orderField.TimeCondition = THOST_FTDC_TC_GFD;//时间条件,当日有效


	//--------------4、数量条件----------------
	orderField.VolumeCondition = THOST_FTDC_VC_AV;//任意数量

	//orderField.VolumeCondition = THOST_FTDC_VC_MV;//最小数量
	//orderField.VolumeCondition = THOST_FTDC_VC_CV;//全部数量




	strcpy(orderField.GTDDate, m_cTradingDay);//下单的日期



	orderField.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;//强平原因

	switch (nOpenClose) {
	case 0:
		orderField.CombOffsetFlag[0] = THOST_FTDC_OF_Open;//开仓
		break;
	case 1:
		orderField.CombOffsetFlag[0] = THOST_FTDC_OF_Close;//平仓
		break;
	case 2:
		orderField.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;//平今仓
		break;
	case 3:
		orderField.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;//平昨仓
		break;
	}


	//调用交易的api的ReqOrderInsert
	int retCode = m_pUserTDApi_trade->ReqOrderInsert(&orderField, GetNextRequestID());

	if (retCode != 0) {
		printf("failed to insert order,instrument: %s, volume: %d, price: %f\n", pszCode, nVolume, fPrice);
	}
}

void TdSpi::CancelOrder(CThostFtdcOrderField* pOrder)
{
	CThostFtdcInputOrderActionField oa;//创建一个撤单的结构体对象
	memset(&oa, 0, sizeof(CThostFtdcInputOrderActionField));//初始化，字段清零

	oa.ActionFlag = THOST_FTDC_AF_Delete;//撤单
	//下面这三个字段，能确定我们的报单
	oa.FrontID = pOrder->FrontID;//前置编号
	oa.SessionID = pOrder->SessionID;//会话
	strcpy(oa.OrderRef, pOrder->OrderRef);//报单引用

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
	//调用交易api的撤单函数
	int nRetCode = m_pUserTDApi_trade->ReqOrderAction(&oa, oa.RequestID);

	char* pszStatus = new char[13];
	switch (pOrder->OrderStatus) {
	case THOST_FTDC_OST_AllTraded:
		strcpy(pszStatus, "全部成交");
		break;
	case THOST_FTDC_OST_PartTradedQueueing:
		strcpy(pszStatus, "部分成交");
		break;
	case THOST_FTDC_OST_NoTradeQueueing:
		strcpy(pszStatus, "未成交");
		break;
	case THOST_FTDC_OST_Canceled:
		strcpy(pszStatus, "已撤单");
		break;
	case THOST_FTDC_OST_Unknown:
		strcpy(pszStatus, "未知");
		break;
	case THOST_FTDC_OST_NotTouched:
		strcpy(pszStatus, "未触发");
		break;
	case THOST_FTDC_OST_Touched:

		strcpy(pszStatus, "已触发");
		break;
	default:
		strcpy(pszStatus, "");
		break;
	}
	/*printf("撤单ing,ins: %s, vol: %d, price:%f, orderref:%s,requestid:%d,traded vol: %d,ExchID:%s, OrderSysID:%s,status: %s,statusmsg:%s\n"
		, pOrder->InstrumentID, pOrder->VolumeTotalOriginal, pOrder->LimitPrice, pOrder->OrderRef, pOrder->RequestID
		, pOrder->VolumeTraded, pOrder->ExchangeID, pOrder->OrderSysID, pszStatus, pOrder->StatusMsg);*/
		//cerr << "TdSpi::CancelOrder 撤单ing" << pszStatus << endl;
	if (nRetCode != 0) {
		printf("cancel order failed.\n");
	}
	else
	{
		pOrder->OrderStatus = '6';//‘6’表示撤单途中
		std::cerr << "TdSpi::CancelOrder 撤单ing" << pszStatus << std::endl;
		std::cerr << "TdSpi::CancelOrder 状态改为 pOrder->OrderStatus :" << pOrder->OrderStatus << std::endl;
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
	CThostFtdcOrderField* pOrder = NULL;//创建一个报单结构体指针
	std::lock_guard<std::mutex> m_lock(m_mutex);//加锁
	std::map<int, CThostFtdcOrderField*>::iterator it = m_Orders.find(nBrokerOrderSeq);//用订单序列号查找订单
	//找到了
	if (it != m_Orders.end()) {
		pOrder = it->second;
	}

	return pOrder;
}

//保存并更新报单的状态
bool TdSpi::UpdateOrder(CThostFtdcOrderField* pOrder)
{
	//经纪公司的下单序列号,大于0表示已经接受报单
	if (pOrder->BrokerOrderSeq > 0)
	{
		std::lock_guard<std::mutex> m_lock(m_mutex);//加锁，保证这个映射数据的安全
		//迭代器，查找是否有这个报单
		std::map<int, CThostFtdcOrderField*>::iterator it = m_Orders.find(pOrder->BrokerOrderSeq);
		//如果存在，我们需要更新它的状态
		if (it != m_Orders.end())
		{
			CThostFtdcOrderField* pOld = it->second;//把结构体的指针赋值给pOld
			//原报单已经关闭；
			char cOldStatus = pOld->OrderStatus;
			switch (cOldStatus)
			{
			case THOST_FTDC_OST_AllTraded://全部成交
			case THOST_FTDC_OST_Canceled://已撤单
			case '6'://canceling//自己定义的，本程序已经发送了撤单请求，还在途中
			case THOST_FTDC_OST_Touched://已经触发
				return false;
			}
			memcpy(pOld, pOrder, sizeof(CThostFtdcOrderField));//更新报单的状态
			std::cerr << "TdSpi::UpdateOrder pOrder->OrderStatus :" << (it->second)->OrderStatus << std::endl;


		}
		//如果不存在，我们需要插入这个报单信息
		else
		{
			CThostFtdcOrderField* pNew = new CThostFtdcOrderField();
			memcpy(pNew, pOrder, sizeof(CThostFtdcOrderField));
			m_Orders.insert(std::pair<int, CThostFtdcOrderField*>(pNew->BrokerOrderSeq, pNew));
		}
		return true;
	}
	//否则的话就不用加入到映射
	return false;
}

bool TdSpi::UpdateOrder(CThostFtdcOrderField* pOrder, std::map<int, CThostFtdcOrderField*>& orderMap)
{
	//经纪公司的下单序列号,大于0表示已经接受报单
	if (pOrder->BrokerOrderSeq > 0)
	{
		std::lock_guard<std::mutex> m_lock(m_mutex);//加锁，保证这个映射数据的安全
		//迭代器，查找是否有这个报单
		std::map<int, CThostFtdcOrderField*>::iterator it = orderMap.find(pOrder->BrokerOrderSeq);
		//如果存在，我们需要更新它的状态
		if (it != orderMap.end())
		{
			CThostFtdcOrderField* pOld = it->second;//把结构体的指针赋值给pOld
			//原报单已经关闭；
			char cOldStatus = pOld->OrderStatus;
			switch (cOldStatus)
			{
			case THOST_FTDC_OST_AllTraded://全部成交
			case THOST_FTDC_OST_Canceled://已撤单
			case '6'://canceling//自己定义的，本程序已经发送了撤单请求，还在途中
			case THOST_FTDC_OST_Touched://已经触发
				return false;
			}
			memcpy(pOld, pOrder, sizeof(CThostFtdcOrderField));//更新报单的状态
			std::cerr << "TdSpi::UpdateOrder pOrder->OrderStatus :" << (it->second)->OrderStatus << std::endl;


		}
		//如果不存在，我们需要插入这个报单信息
		else
		{
			CThostFtdcOrderField* pNew = new CThostFtdcOrderField();
			memcpy(pNew, pOrder, sizeof(CThostFtdcOrderField));
			orderMap.insert(std::pair<int, CThostFtdcOrderField*>(pNew->BrokerOrderSeq, pNew));
		}
		return true;
	}
	//否则的话就不用加入到映射
	return false;
}

int TdSpi::GetNextRequestID()
{
	//给m_nNextRequestID加上互斥锁
	/*m_mutex.lock();
	int nNextID = m_nNextRequestID++;
	m_mutex.unlock();*/
	//1原理，在构造函数里面使用m_mutex.lock();
	//在析构的时候使用解锁m_mutex.unlock();

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

		std::cerr << "响应 | 合约：" << pInstrument->InstrumentID
			<< "合约名称：" << pInstrument->InstrumentName
			<< " 合约在交易所代码：" << pInstrument->ExchangeInstID
			<< " 产品代码：" << pInstrument->ProductID
			<< " 产品类型：" << pInstrument->ProductClass
			<< " 多头保证金率：" << pInstrument->LongMarginRatio
			<< " 空头保证金率：" << pInstrument->ShortMarginRatio
			<< " 合约数量乘数：" << pInstrument->VolumeMultiple
			<< " 最小变动价位：" << pInstrument->PriceTick
			<< " 交易所代码：" << pInstrument->ExchangeID
			<< " 交割年份：" << pInstrument->DeliveryYear
			<< " 交割月：" << pInstrument->DeliveryMonth
			<< " 创建日：" << pInstrument->CreateDate
			<< " 到期日：" << pInstrument->ExpireDate
			<< " 上市日：" << pInstrument->OpenDate
			<< " 开始交割日：" << pInstrument->StartDelivDate
			<< " 结束交割日：" << pInstrument->EndDelivDate
			<< " 合约生命周期状态：" << pInstrument->InstLifePhase
			<< " 当前是否交易：" << pInstrument->IsTrading << std::endl;
	}
}

bool TdSpi::FindTrade(CThostFtdcTradeField* pTrade)
{
	//ExchangeID //交易所代码
	//	TradeID   //成交编号
	//	Direction  //买卖方向
	std::lock_guard<std::mutex> m_lock(m_mutex);//加锁，保证这个set数据的安全
	for (auto it = tradeList.begin(); it != tradeList.end(); it++)
	{
		//判断是否已经存在
		if (strcmp((*it)->ExchangeID, pTrade->ExchangeID) == 0 &&
			strcmp((*it)->TradeID, pTrade->TradeID) == 0 && (*it)->Direction == pTrade->Direction)
			return true;
	}

	return false;
}

void TdSpi::InsertTrade(CThostFtdcTradeField* pTrade)
{
	std::lock_guard<std::mutex> m_lock(m_mutex);//加锁，保证这个set数据的安全
	CThostFtdcTradeField* trade = new CThostFtdcTradeField();//创建trade，分配堆空间，记得在析构函数里面要delete
	memcpy(trade, pTrade, sizeof(CThostFtdcTradeField));//pTrade复制给trade
	tradeList.push_back(trade);//输入录入
}

void TdSpi::ShowTradeList()
{
	std::lock_guard<std::mutex> m_lock(m_mutex);//加锁，保证这个set数据的安全
	std::cerr << std::endl << std::endl;
	std::cerr << "---------------打印成交明细-------------" << std::endl;
	for (auto iter = tradeList.begin(); iter != tradeList.end(); iter++)
	{
		std::cerr << std::endl << "投资者代码：" << (*iter)->InvestorID << "  "
			<< "用户代码：" << (*iter)->UserID << "  " << "成交编号：" << (*iter)->TradeID << "  "
			<< "合约代码：" << (*iter)->InstrumentID << "  " << "买卖方向：" << (*iter)->Direction << "  "
			<< "开平：" << (*iter)->OffsetFlag << "  " << "投机/套保" << (*iter)->HedgeFlag << "  "
			<< "价格：" << (*iter)->Price << "  " << "数量：" << (*iter)->Volume << "  "
			<< "报单引用：" << (*iter)->OrderRef << "  " << "本地报单编号：" << (*iter)->OrderLocalID << "  "
			<< "成交时间：" << (*iter)->TradeTime << "  " << "业务单元：" << (*iter)->BusinessUnit << "  "
			<< "序号：" << (*iter)->SequenceNo << "  " << "经纪公司下单序号：" << (*iter)->BrokerOrderSeq << "  "
			<< "交易日：" << (*iter)->TradingDay << std::endl;
	}

	std::cerr << std::endl << std::endl;

}

void TdSpi::ShowOrderList(std::map<int, CThostFtdcOrderField*>& orderList)
{
	if (orderList.size() > 0)
	{
		std::cerr << std::endl << std::endl << std::endl;
		std::cerr << "总的报单次数：" << orderList.size() << std::endl;
		std::cerr << "---------------打印报单开始---------------" << std::endl;
		for (auto iter = orderList.begin(); iter != orderList.end(); iter++)
		{
			std::cerr << "经纪公司代码：" << (iter->second)->BrokerID << "  " << "投资者代码：" << (iter->second)->InvestorID << "  "
				<< "用户代码：" << (iter->second)->UserID << "  " << "合约代码：" << (iter->second)->InstrumentID << "  "
				<< "买卖方向：" << (iter->second)->Direction << "  " << "组合开平标志：" << (iter->second)->CombOffsetFlag << "  "
				<< "价格：" << (iter->second)->LimitPrice << "  " << "数量：" << (iter->second)->VolumeTotalOriginal << "  "
				<< "报单引用：" << (iter->second)->OrderRef << "  " << "客户代码：" << (iter->second)->ClientID << "  "
				<< "报单状态：" << (iter->second)->OrderStatus << "  " << "委托时间：" << (iter->second)->InsertTime << "  "
				<< "报单编号：" << (iter->second)->OrderStatus << "  " << "交易日：" << (iter->second)->TradingDay << "  "
				<< "报单日期：" << (iter->second)->InsertDate << std::endl;

		}
		std::cerr << "---------------打印报单完成---------------" << std::endl;
		std::cerr << std::endl << std::endl << std::endl;
	}
}

void TdSpi::Release()
{
	//清空orderList，当天的所有报单数据
	for (auto it = orderList.begin(); it != orderList.end(); it++)
	{
		delete it->second;
		it->second = nullptr;
	}
	orderList.clear();

	//当天的所有成交
	//std::vector<CThostFtdcTradeField*> tradeList;
	for (auto it = tradeList.begin(); it != tradeList.end(); it++)
	{
		delete (*it);
		*it = nullptr;
	}
	tradeList.clear();
}
