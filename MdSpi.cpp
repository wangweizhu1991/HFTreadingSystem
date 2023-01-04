#include "MdSpi.h"
#include <map>
#include <iostream>
#include <mutex>
#include <vector>
#include "TdSpi.h"

extern std::map<std::string, std::string> accountConfig_map;//保存账户信息的map
//全局的互斥锁
extern std::mutex m_mutex;

//全局的requestId
extern int g_nRequestID;

//全局持仓合约
extern std::vector<std::string> subscribe_inst_vec;

extern TdSpi* g_pUserTDSpi_AI;//全局的TD回调处理类对象，AI交互函数会用到

int nCount = 0;

MdSpi::MdSpi(CThostFtdcMdApi* mdapi):mdapi(mdapi)
{
	//登录参数数据
	//期货公司代码
	m_BrokerId = accountConfig_map["brokerId"];
	//期货账户
	m_UserId = accountConfig_map["userId"];
	//密码
	m_Passwd = accountConfig_map["passwd"];
	//策略需要交易的合约
	memset(m_InstId, 0, sizeof(m_InstId));
	strcpy(m_InstId, accountConfig_map["contract"].c_str());
}

MdSpi::~MdSpi()
{
	//这几个是new出来的在堆区。所以用完在析构函数中需要删除
	if (m_InstIdList_all)
		delete[] m_InstIdList_all;

	if (m_InstIdList_Position_MD)
		delete[] m_InstIdList_Position_MD;

	if (loginField)
		delete loginField;
}

void MdSpi::OnFrontConnected()//前置连接
{
	std::cerr << "行情前置已连接上，请求登录" << std::endl;

	ReqUserLogin(m_BrokerId, m_UserId, m_Passwd);

}

//请求登录行情
void MdSpi::ReqUserLogin(std::string brokerId, std::string userId, std::string passwd)
{
	loginField = new CThostFtdcReqUserLoginField();
	strcpy(loginField->BrokerID, brokerId.c_str());
	strcpy(loginField->UserID, userId.c_str());
	strcpy(loginField->Password, passwd.c_str());
	//错误的brokerid
	//strcpy(loginField->BrokerID, "0000");
	//strcpy(loginField->UserID, "");//BrokerID、账号密码可以不填（填错也没事），也可以连接到行情，就像软件上的单行情登录
	//strcpy(loginField->Password, "");
	int nResult = mdapi->ReqUserLogin(loginField, GetNextRequestID());
	std::cerr << "请求登录行情：" << ((nResult == 0) ? "成功" : "失败") << std::endl;
}

void MdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	std::cerr << "OnRspUserLogin 行情登陆成功! " << std::endl;;
	std::cerr << "交易日：" << mdapi->GetTradingDay() << std::endl;
	if (pRspInfo->ErrorID == 0) {
		std::cerr << "请求的登陆成功," << "请求ID为" << nRequestID << std::endl;
		/***************************************************************/
		std::cerr << "尝试订阅行情" << std::endl;

		////插入合约到订阅列表
		InsertInstToSubVec(m_InstId);

		////订阅“量化策略交易的合约”行情
		//SubscribeMarketData(m_InstId);

		////方式一：以下订阅行情使用vector的方式
		//if (subscribe_inst_vec.size() > 0)
		//{
		//	//"IF2012,IF2101,IF2103"
		//	std::cerr << "订阅行情的合约数量：" << subscribe_inst_vec.size() << std::endl;
		//	std::cerr << "有持仓，订阅行情  " << std::endl;
		//	SubscribeMarketData(subscribe_inst_vec);
		//
		//}
		////以上订阅行情使用vector的方式


		////方式二：以下订阅行情使用char的方式
		//if (m_InstIdList_Position_MD)
		//{
		//	std::cerr << "m_InstIdList_Position_MD 大小：" << strlen(m_InstIdList_Position_MD) << "," << m_InstIdList_Position_MD << std::endl;
		//	std::cerr << "有持仓，订阅行情  " << std::endl;
		//	SubscribeMarketData(m_InstIdList_Position_MD);//订阅行的流控6笔/秒，没有持仓就不用订阅行情
		//}
		////以上订阅行情使用char的方式

		////else语句方式一和方式二都可以用上
		//else
		//{
		//	std::cerr << "当前没有持仓" << std::endl;
		//}


		////方式三：以下订阅行情使用直接输入的方式
		//SubscribeMarketData("IC2306,IF2301");
		SubscribeMarketData("ni2301");
		////以上订阅行情使用char的方式


		//方式四：订阅所有合约
		//SubscribeMarketData_All();
		//以上订阅行情所有合约的方式


		//SubscribeMarketData("IG6666");//写错了合约代码订阅行情没有错误提示。
		//SubscribeMarketData("IC2301");//多次重复订阅，不会报错，但同一合约订阅超过6次就没有回报了。因此。每秒订阅合约不能超过6次。
		//SubscribeMarketData("IC2301");
		//SubscribeMarketData("IC2301");


		//策略启动后默认禁止开仓
		std::cerr << std::endl << std::endl << std::endl << "策略默认禁止开仓，如果允许交易，请输入指令（允许开仓：yxkc,禁止开仓：jzkc）" << std::endl;
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
	//char*的vetor
	std::vector<char*>list;
	/*   strtok()用来将字符串分割成一个个片段。参数s指向欲分割的字符串，参数delim则为分割字符串中包含的所有字符。
		当strtok()在参数s的字符串中发现参数delim中包含的分割字符时, 则会将该字符改为\0 字符。
		在第一次调用时，strtok()必需给予参数s字符串，往后的调用则将参数s设置成NULL。
		每次调用成功则返回指向被分割出片段的指针。*/
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
		ppInstrument[i] = list[i];//指针赋值，没有新分配内存空间
	}
	//调用行情api的SubscribeMarketData
	int nRet = mdapi->SubscribeMarketData(ppInstrument, len);
	std::cerr << "请求订阅行情：" << ((nRet == 0) ? "成功" : "失败") << std::endl;


	delete[] ppInstrument;
}

void MdSpi::SubscribeMarketData(std::string instIdList)
{
	int len = instIdList.size();
	//分配len+1个char的空间，否则会报错
	char* pInst = new char[len + 1];
	strcpy(pInst, instIdList.c_str());
	//不要忘记加上结尾标志
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
		std::cerr << "订阅行情 " << (nResult == 0 ? ("成功") : ("失败")) << std::endl;

		for (int i = 0; i < nLen; i++)
			delete[] ppInst[i];

		delete[] ppInst;
	}
}

void MdSpi::SubscribeMarketData_All()
{
	SubscribeMarketData(m_InstIdList_all);
}

//深度行情报价响应，一秒2次刷新
void MdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* pDepthMarketData)
{
	std::cout << "===========================================" << std::endl;
	std::cout << "深度行情" << std::endl;
	std::cout << " 交易日:" << pDepthMarketData->TradingDay << std::endl
		<< "合约代码:" << pDepthMarketData->InstrumentID << std::endl
		<< "最新价:" << pDepthMarketData->LastPrice << std::endl
		<< "上次结算价:" << pDepthMarketData->PreSettlementPrice << std::endl
		<< "昨收盘:" << pDepthMarketData->PreClosePrice << std::endl
		<< "数量:" << pDepthMarketData->Volume << std::endl
		<< "昨持仓量:" << pDepthMarketData->PreOpenInterest << std::endl
		<< "最后更新时间" << pDepthMarketData->UpdateTime << std::endl
		<< "最后更新毫秒" << pDepthMarketData->UpdateMillisec << std::endl
	<< "申买价一：" << pDepthMarketData->BidPrice1 << std::endl
	<< "申买量一:" << pDepthMarketData->BidVolume1 << std::endl
	<< "申卖价一:" << pDepthMarketData->AskPrice1 << std::endl
	<< "申卖量一:" << pDepthMarketData->AskVolume1 << std::endl
	<< "今收盘价:" << pDepthMarketData->ClosePrice << std::endl
	<< "当日均价:" << pDepthMarketData->AveragePrice << std::endl
	<< "本次结算价格:" << pDepthMarketData->SettlementPrice << std::endl
	<< "成交金额:" << pDepthMarketData->Turnover << std::endl
	<< "持仓量:" << pDepthMarketData->OpenInterest << std::endl;

	if (nCount++ < 1)
		g_pUserTDSpi_AI->PlaceOrder(pDepthMarketData->InstrumentID, pDepthMarketData->ExchangeID, 0, 0, 1, pDepthMarketData->LowerLimitPrice);//测试买入开仓1手

}

//例子："IF2102,IF2103",string类型转换为char *
void MdSpi::setInstIdList_Position_MD(std::string& inst_holding)
{
	int len = inst_holding.length();
	m_InstIdList_Position_MD = new char[len + 1];

	memcpy(m_InstIdList_Position_MD, inst_holding.c_str(), len);
	m_InstIdList_Position_MD[len] = '\0';
}

void MdSpi::InsertInstToSubVec(char* Inst)
{
	//将数据插入到vector里面  subscribe_inst_vec;
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
	//如果没有找到就插入订阅合约的vector中
	if (!findflag)
		subscribe_inst_vec.push_back(Inst);
}

/// <summary>
/// 将string类型的所有合约转换为char *类型
/// </summary>
/// <param name="inst_all"></param>
///string inst_all="IF2012,IF2101,IF2103";
void MdSpi::set_InstIdList_All(std::string& inst_all)
{
	//字符串的长度
	int nLen = inst_all.size();
	//
	m_InstIdList_all = new char[nLen + 1];
	strcpy(m_InstIdList_all, inst_all.c_str());
	//千万记住加上结束标志，重要，否则会出错
	m_InstIdList_all[nLen] = '\0';
	//delete [] m_InstIdList_all;写到类的析构函数中
}

int MdSpi::GetNextRequestID()
{
	//给m_nNextRequestID加上互斥锁
/*m_mutex.lock();
int nNextID = m_nNextRequestID++;
m_mutex.unlock();*/
//1原理，在构造函数里面使用m_mutex.lock();
//在析构的时候使用解锁m_mutex.unlock();
std::lock_guard<std::mutex> m_lock(m_mutex);

	int nNextID = g_nRequestID++;

	return g_nRequestID;
}
