/*
 * mmwave-enb-phy.h
 *
 *  Created on: Nov 4, 2014
 *      Author: sourjya
 */

#ifndef SRC_MMWAVE_MODEL_MMWAVE_ENB_PHY_H_
#define SRC_MMWAVE_MODEL_MMWAVE_ENB_PHY_H_


#include "mmwave-phy.h"
#include "mmwave-phy-mac-common.h"
#include "mmwave-control-messages.h"
#include "mmwave-mac.h"
#include <ns3/lte-enb-phy-sap.h>
#include <ns3/lte-enb-cphy-sap.h>
#include <ns3/mmwave-harq-phy.h>

namespace ns3{

typedef std::pair<uint64_t, uint64_t > pairDevices_t;

class PacketBurst;
class MmWaveNetDevice;
class MmWaveUePhy;
class MmWaveEnbMac;

class MmWaveEnbPhy : public MmWavePhy
{
	friend class MemberLteEnbCphySapProvider<MmWaveEnbPhy>;
public:
	MmWaveEnbPhy ();

	MmWaveEnbPhy (Ptr<MmWaveSpectrumPhy> , Ptr<MmWaveSpectrumPhy>);
	virtual ~MmWaveEnbPhy ();

	static TypeId GetTypeId (void);
	virtual void DoInitialize (void);
	virtual void DoDispose (void);

	void SetMmWaveEnbCphySapUser (LteEnbCphySapUser* s);
	LteEnbCphySapProvider* GetMmWaveEnbCphySapProvider ();

	void SetTxPower (double pow);
	double GetTxPower () const;

	void SetNoiseFigure (double pf);
	double GetNoiseFigure () const;

	void CalcChannelQualityForUe (std::vector <double> sinr, Ptr<MmWaveSpectrumPhy> ue);

	virtual Ptr<SpectrumValue> CreateTxPowerSpectralDensity ();

	void SetSubChannels (std::vector<int> mask );

	void DoSetSubChannels ();

	void StartSubFrame (void);
	void StartSlot (void);

	void EndSlot (void);
	void EndSubFrame (void);

	void SendDataChannels (Ptr<PacketBurst> pb, Time slotPrd, SlotAllocInfo& slotInfo);

	void SendCtrlChannels (std::list<Ptr<MmWaveControlMessage> > ctrlMsg, Time slotPrd);

	Ptr<MmWaveSpectrumPhy> GetDlSpectrumPhy () const;
	Ptr<MmWaveSpectrumPhy> GetUlSpectrumPhy () const;

	/**virtual void SendIdealControlMessage(Ptr<IdealControlMessage> msg);
	virtual void ReceiveIdealControlMessage(Ptr<IdealControlMessage> msg)**/

	bool AddUePhy (uint64_t imsi, Ptr<NetDevice> ueDevice);

//	void SetMacPdu (Ptr<Packet> pb);

	void PhyDataPacketReceived (Ptr<Packet> p);

	void GenerateDataCqiReport (const SpectrumValue& sinr);

	void PhyCtrlMessagesReceived (std::list<Ptr<MmWaveControlMessage> > msgList);

	uint32_t GetAbsoluteSubframeNo (); // Used for tracing purposes

	int8_t DoGetReferenceSignalPower () const;

	void SetPhySapUser (MmWaveEnbPhySapUser* ptr);

	void SetHarqPhyModule (Ptr<MmWaveHarqPhy> harq);

	void ReceiveUlHarqFeedback (UlHarqInfo mes);

	void UpdateUeSinrEstimate ();

	std::vector<double> AddGaussianNoise(std::vector<double>);

	std::pair <uint64_t,uint64_t> ApplyFilter(std::vector<double>);

	double MakeAvg(std::vector<double>);

	double MakeVar(std::vector<double>, double);

	std::vector<double> MakeFilter (std::vector<double> , std::vector<double> , std::pair <uint64_t , uint64_t > );



private:

	bool AddUePhy (uint16_t rnti);
	// LteEnbCphySapProvider forwarded methods
	void DoSetBandwidth (uint8_t ulBandwidth, uint8_t dlBandwidth);
	void DoSetEarfcn (uint16_t dlEarfcn, uint16_t ulEarfcn);
	void DoAddUe (uint16_t rnti);
	void DoRemoveUe (uint16_t rnti);
	void DoSetPa (uint16_t rnti, double pa);
	void DoSetTransmissionMode (uint16_t  rnti, uint8_t txMode);
	void DoSetSrsConfigurationIndex (uint16_t  rnti, uint16_t srcCi);
	void DoSetMasterInformationBlock (LteRrcSap::MasterInformationBlock mib);
	void DoSetSystemInformationBlockType1 (LteRrcSap::SystemInformationBlockType1 sib1);

	void DoSetBandwidth (uint8_t Bandwidth );
	void DoSetEarfcn (uint16_t Earfcn );

	void QueueUlTbAlloc (TbAllocInfo tbAllocInfo);
	std::list<TbAllocInfo> DequeueUlTbAlloc ();

	uint8_t m_currSfNumSlots;

  uint32_t m_numRbg;

	std::set <uint64_t> m_ueAttached;

	std::vector <int> m_listOfSubchannels;

	uint8_t m_prevSlot; // 1->UL 0->DL 2->Unspecified

	SlotAllocInfo::TddMode m_prevSlotDir;

	std::vector< Ptr<NetDevice> > m_deviceMap;

	MmWaveEnbPhySapUser* m_phySapUser;

	LteEnbCphySapProvider* m_enbCphySapProvider;
	LteEnbCphySapUser* m_enbCphySapUser;
	LteRrcSap::SystemInformationBlockType1 m_sib1;
	std::set <uint16_t> m_ueAttachedRnti;
	std::map <uint64_t, Ptr<NetDevice> > m_ueAttachedImsiMap;
	std::map <uint64_t, double > m_sinrMap;
	std::map <uint64_t, Ptr<SpectrumValue> > m_rxPsdMap;
	std::map <pairDevices_t , std::vector<double> > m_sinrVector; // array containing all SINR values for a specific pair (UE-eNB)
	std::map <pairDevices_t , std::vector<double> > m_sinrVectorNoisy; // array containing all noisy SINR values for a specific pair (UE-eNB)
	std::map <pairDevices_t , std::vector<double> > m_finalSinrVector; // array containing all  SINR values after the filtering for a specific pair (UE-eNB)
	std::map <pairDevices_t, std::pair <uint64_t,uint64_t> > m_samplesFilter; // array containing all noisy SINR values for a specific pair (UE-eNB)

	int m_updateSinrPeriod; // the period of SINR update for eNBs
	double m_ueUpdateSinrPeriod; // the period of SINR reporting to the UEs
	double m_updateSinrCollect; // the period of SINR collection, for a pair (UE-eNB)
	uint16_t m_roundFromLastUeSinrUpdate; // the ratio between the two above

	Ptr<MmWaveHarqPhy> m_harqPhyModule;
	std::vector <int> m_channelChunks;

	Time m_sfPeriod;
	Time m_lastSfStart;

	uint8_t m_currSymStart;

	TracedCallback< uint64_t, SpectrumValue&, SpectrumValue& > m_ulSinrTrace;
};

}


#endif /* SRC_MMWAVE_MODEL_MMWAVE_ENB_PHY_H_ */
