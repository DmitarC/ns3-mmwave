/*
 * mmwave-ue-phy.cc
 *
 *  Created on: Nov 5, 2014
 *      Author: sourjya
 */

#include <ns3/object-factory.h>
#include <ns3/log.h>
#include <cfloat>
#include <cmath>
#include <ns3/simulator.h>
#include <ns3/double.h>
#include "mmwave-ue-phy.h"
#include "mmwave-ue-net-device.h"
#include "mmwave-spectrum-value-helper.h"
#include <ns3/pointer.h>


namespace ns3{

NS_LOG_COMPONENT_DEFINE ("MmWaveUePhy");

NS_OBJECT_ENSURE_REGISTERED (MmWaveUePhy);

MmWaveUePhy::MmWaveUePhy ()
{
	NS_LOG_FUNCTION (this);
	NS_FATAL_ERROR ("This constructor should not be called");
}

MmWaveUePhy::MmWaveUePhy (Ptr<MmWaveSpectrumPhy> dlPhy, Ptr<MmWaveSpectrumPhy> ulPhy)
: MmWavePhy(dlPhy, ulPhy),
  m_prevSlot (0),
  m_rnti (0)
{
	NS_LOG_FUNCTION (this);
	m_wbCqiLast = Simulator::Now ();
	m_ueCphySapProvider = new MemberLteUeCphySapProvider<MmWaveUePhy> (this);
	m_pucchSlotInd = 2; // default slot containing dedicated UL control channel
	Simulator::ScheduleNow (&MmWaveUePhy::SubframeIndication, this, 0, 0);
}

MmWaveUePhy::~MmWaveUePhy ()
{
	NS_LOG_FUNCTION (this);
}

TypeId
MmWaveUePhy::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::MmWaveUePhy")
	    .SetParent<MmWavePhy> ()
	    .AddConstructor<MmWaveUePhy> ()
	    .AddAttribute ("TxPower",
	                   "Transmission power in dBm",
	                   DoubleValue (30.0), //TBD zml
	                   MakeDoubleAccessor (&MmWaveUePhy::SetTxPower,
	                                       &MmWaveUePhy::GetTxPower),
	                   MakeDoubleChecker<double> ())
		.AddAttribute ("DlSpectrumPhy",
					    "The downlink MmWaveSpectrumPhy associated to this MmWavePhy",
					    TypeId::ATTR_GET,
					    PointerValue (),
					    MakePointerAccessor (&MmWaveUePhy::GetDlSpectrumPhy),
					    MakePointerChecker <MmWaveSpectrumPhy> ())
		.AddAttribute ("UlSpectrumPhy",
					    "The uplink MmWaveSpectrumPhy associated to this MmWavePhy",
					    TypeId::ATTR_GET,
					    PointerValue (),
					    MakePointerAccessor (&MmWaveUePhy::GetUlSpectrumPhy),
					    MakePointerChecker <MmWaveSpectrumPhy> ())
		.AddTraceSource ("ReportCurrentCellRsrpSinr",
						 "RSRP and SINR statistics.",
						 MakeTraceSourceAccessor (&MmWaveUePhy::m_reportCurrentCellRsrpSinrTrace),
	                     "ns3::mmWaveUePhy::RsrpRsrqTracedCallback")
		.AddTraceSource ("ReportUplinkTbSize",
						 "Report allocated uplink TB size for trace.",
						 MakeTraceSourceAccessor (&MmWaveUePhy::m_reportUlTbSize),
						 "ns3::mmWaveUePhy::UlTbSizeTracedCallback")
		.AddTraceSource ("ReportDownlinkTbSize",
						 "Report allocated downlink TB size for trace.",
						 MakeTraceSourceAccessor (&MmWaveUePhy::m_reportDlTbSize),
						 "ns3::mmWaveUePhy::DlTbSizeTracedCallback")
;

	return tid;
}

void
MmWaveUePhy::DoInitialize (void)
{
	NS_LOG_FUNCTION (this);
	m_dlCtrlPeriod = NanoSeconds (1000 * m_phyMacConfig->GetDlCtrlSymbols() * m_phyMacConfig->GetSymbolPeriod());
	m_ulCtrlPeriod = NanoSeconds (1000 * m_phyMacConfig->GetUlCtrlSymbols() * m_phyMacConfig->GetSymbolPeriod());

	//m_dataPeriod = NanoSeconds (1000 * (m_phyMacConfig->GetSymbPerSlot() - m_phyMacConfig->GetCtrlSymbols()) * m_phyMacConfig->GetSymbolPeriod());
	//m_numRbg = m_phyMacConfig->GetNumRb () / m_phyMacConfig->GetNumRbPerRbg ();
	//m_ulTbAllocQueue.resize (m_phyMacConfig->GetUlSchedDelay());
	//m_currSfAllocInfo = SfAllocInfo (m_phyMacConfig->GetSlotsPerSubframe ());

	//m_sfAllocInfo.resize (m_phyMacConfig->GetSubframesPerFrame());
	for (unsigned i = 0; i < m_phyMacConfig->GetSubframesPerFrame(); i++)
	{
		m_sfAllocInfo.push_back(SfAllocInfo(SfnSf (0, i, 0)));
		SlotAllocInfo dlCtrlSlot;
		dlCtrlSlot.m_numCtrlSym = 1;
		dlCtrlSlot.m_tddMode = SlotAllocInfo::DL;
		dlCtrlSlot.m_dci.m_numSym = 1;
		dlCtrlSlot.m_dci.m_symStart = 0;
		SlotAllocInfo ulCtrlSlot;
		ulCtrlSlot.m_numCtrlSym = 1;
		ulCtrlSlot.m_tddMode = SlotAllocInfo::UL;
		ulCtrlSlot.m_slotIdx = 0xFF;
		ulCtrlSlot.m_dci.m_numSym = 1;
		ulCtrlSlot.m_dci.m_symStart = m_phyMacConfig->GetSymbolsPerSubframe()-1;
		m_sfAllocInfo[i].m_dlSlotAllocInfo.push_back (dlCtrlSlot);
		m_sfAllocInfo[i].m_ulSlotAllocInfo.push_back (ulCtrlSlot);
	}

	for (unsigned i = 0; i < m_phyMacConfig->GetTotalNumChunk(); i++)
	{
		m_channelChunks.push_back(i);
	}

	m_sfPeriod = NanoSeconds (1000.0 * m_phyMacConfig->GetSubframePeriod ());

	MmWavePhy::DoInitialize ();
}

void
MmWaveUePhy::DoDispose (void)
{

}

void
MmWaveUePhy::SetUeCphySapUser (LteUeCphySapUser* s)
{
  NS_LOG_FUNCTION (this);
  m_ueCphySapUser = s;
}

LteUeCphySapProvider*
MmWaveUePhy::GetUeCphySapProvider ()
{
  NS_LOG_FUNCTION (this);
  return (m_ueCphySapProvider);
}

void
MmWaveUePhy::SetTxPower (double pow)
{
	m_txPower = pow;
}
double
MmWaveUePhy::GetTxPower () const
{
	return m_txPower;
}

void
MmWaveUePhy::SetNoiseFigure (double pf)
{

}

double
MmWaveUePhy::GetNoiseFigure () const
{
	return m_noiseFigure;
}

Ptr<SpectrumValue>
MmWaveUePhy::CreateTxPowerSpectralDensity()
{
	Ptr<SpectrumValue> psd =
				MmWaveSpectrumValueHelper::CreateTxPowerSpectralDensity (m_phyMacConfig, m_txPower, m_subChannelsForTx );
	return psd;
}

void
MmWaveUePhy::DoSetSubChannels()
{

}

void
MmWaveUePhy::SetSubChannelsForReception(std::vector <int> mask)
{

}

std::vector <int>
MmWaveUePhy::GetSubChannelsForReception(void)
{
	std::vector <int> vec;

	return vec;
}

void
MmWaveUePhy::SetSubChannelsForTransmission(std::vector <int> mask)
{
	m_subChannelsForTx = mask;
	Ptr<SpectrumValue> txPsd = CreateTxPowerSpectralDensity ();
	NS_ASSERT (txPsd);
	m_downlinkSpectrumPhy->SetTxPowerSpectralDensity (txPsd);
}

std::vector <int>
MmWaveUePhy::GetSubChannelsForTransmission(void)
{
	std::vector <int> vec;

	return vec;
}

void
MmWaveUePhy::DoSendControlMessage (Ptr<MmWaveControlMessage> msg)
{
  NS_LOG_FUNCTION (this << msg);
  SetControlMessage (msg);
}


void
MmWaveUePhy::RegisterToEnb (uint16_t cellId, Ptr<MmWavePhyMacCommon> config)
{
	m_cellId = cellId;
	//TBD how to assign bandwitdh and earfcn
	m_noiseFigure = 5.0;
	m_phyMacConfig = config;

	Ptr<SpectrumValue> noisePsd =
			MmWaveSpectrumValueHelper::CreateNoisePowerSpectralDensity (m_phyMacConfig, m_noiseFigure);
	m_downlinkSpectrumPhy->SetNoisePowerSpectralDensity (noisePsd);
	m_downlinkSpectrumPhy->GetSpectrumChannel()->AddRx(m_downlinkSpectrumPhy);
	m_downlinkSpectrumPhy->SetCellId(m_cellId);
}

Ptr<MmWaveSpectrumPhy>
MmWaveUePhy::GetDlSpectrumPhy () const
{
  return m_downlinkSpectrumPhy;
}

Ptr<MmWaveSpectrumPhy>
MmWaveUePhy::GetUlSpectrumPhy () const
{
  return m_uplinkSpectrumPhy;
}

void
MmWaveUePhy::ReceiveControlMessageList (std::list<Ptr<MmWaveControlMessage> > msgList)
{
	NS_LOG_FUNCTION (this);

	std::list<Ptr<MmWaveControlMessage> >::iterator it;
	for (it = msgList.begin (); it != msgList.end (); it++)
	{
		Ptr<MmWaveControlMessage> msg = (*it);

		if (msg->GetMessageType() == MmWaveControlMessage::DCI_TDMA)
		{
<<<<<<< HEAD
			NS_ASSERT_MSG (m_slotNum == 0, "UE" << m_rnti << " got DCI on slot != 0");
			Ptr<MmWaveTdmaDciMessage> dciMsg = DynamicCast<MmWaveTdmaDciMessage> (msg);
			DciInfoElementTdma dciInfoElem = dciMsg->GetDciInfoElement ();
			SfnSf dciSfn = dciMsg->GetSfnSf ();

			if(dciSfn.m_frameNum != m_frameNum || dciSfn.m_sfNum != m_sfNum)
=======
			Ptr<MmWaveDciMessage> dciMsg = DynamicCast<MmWaveDciMessage> (msg);
			DciInfoElement dciInfoElem = dciMsg->GetDciInfoElement ();

			Ptr<MmWaveDciMessage> dci = DynamicCast<MmWaveDciMessage> (msg);
			unsigned dciSfn = dci->GetSfnSf ();
			unsigned dciFrame = ((dciSfn >> 16) & 0x3FF);
			unsigned dciSf = ((dciSfn >> 8) & 0xFF);
			if(dciFrame != m_nrFrames || dciSf != sfInd)
			{
				NS_FATAL_ERROR ("DCI intended for different subframe (dci= " << dciFrame<<" "<<dciSf<<", actual= "<<m_nrFrames<<" "<<sfInd);
			}

			if (dciInfoElem.m_rnti != m_rnti)
>>>>>>> b36c54152804fd16ccc9464c4123d6ceeba3fb48
			{
				NS_FATAL_ERROR ("DCI intended for different subframe (dci= "
						<< dciSfn.m_frameNum<<" "<<dciSfn.m_sfNum<<", actual= "<<m_frameNum<<" "<<m_sfNum);
			}

<<<<<<< HEAD
//			NS_LOG_DEBUG ("UE" << m_rnti << " DCI received for RNTI " << dciInfoElem.m_rnti << " in frame " << m_frameNum << " subframe " << (unsigned)m_sfNum << " slot " << (unsigned)m_slotNum << " format " << (unsigned)dciInfoElem.m_format << " symStart " << (unsigned)dciInfoElem.m_symStart << " numSym " << (unsigned)dciInfoElem.m_numSym);

=======
			NS_LOG_DEBUG ("UE " << m_rnti << " DCI received in frame " << m_nrFrames << " subframe " << sfInd << " slot " << slotInd << " nTB " << dciInfoElem.m_tbInfoElements.size() );
>>>>>>> b36c54152804fd16ccc9464c4123d6ceeba3fb48

			if (dciInfoElem.m_rnti != m_rnti)
			{
				continue; // DCI not for me
			}

			if (dciInfoElem.m_format == DciInfoElementTdma::DL) // set downlink slot schedule for current slot
			{
<<<<<<< HEAD
				NS_LOG_DEBUG ("UE" << m_rnti << " DL-DCI received for frame " << m_frameNum << " subframe " << (unsigned)m_sfNum
								<< " symStart " << (unsigned)dciInfoElem.m_symStart << " numSym " << (unsigned)dciInfoElem.m_numSym  << " tbs " << dciInfoElem.m_tbSize
								<< " harqId " << (unsigned)dciInfoElem.m_harqProcess);

				SlotAllocInfo slotInfo;
				slotInfo.m_tddMode = SlotAllocInfo::DL;
				slotInfo.m_dci = dciInfoElem;
				slotInfo.m_slotIdx = m_sfAllocInfo[m_sfNum].m_dlSlotAllocInfo.size ();
				m_currSfAllocInfo.m_dlSlotAllocInfo.push_back (slotInfo);  // add SlotAllocInfo to current SfAllocInfo
			}
			else if (dciInfoElem.m_format == DciInfoElementTdma::UL) // set downlink slot schedule for t+Tul_sched slot
			{
				uint8_t ulSfIdx = (m_sfNum + m_phyMacConfig->GetUlSchedDelay()) % m_phyMacConfig->GetSubframesPerFrame ();
				uint16_t dciFrame = (ulSfIdx > m_sfNum) ? m_frameNum : m_frameNum+1;

				NS_LOG_DEBUG ("UE" << m_rnti << " UL-DCI received for frame " << dciFrame << " subframe " << (unsigned)ulSfIdx
						     << " symStart " << (unsigned)dciInfoElem.m_symStart << " numSym " << (unsigned)dciInfoElem.m_numSym << " tbs " << dciInfoElem.m_tbSize
						     << " harqId " << (unsigned)dciInfoElem.m_harqProcess);

				SlotAllocInfo slotInfo;
				slotInfo.m_tddMode = SlotAllocInfo::UL;
				slotInfo.m_dci = dciInfoElem;
				SlotAllocInfo ulCtrlSlot = m_sfAllocInfo[ulSfIdx].m_ulSlotAllocInfo.back ();
				m_sfAllocInfo[ulSfIdx].m_ulSlotAllocInfo.pop_back ();
				//ulCtrlSlot.m_slotIdx++;
				slotInfo.m_slotIdx = m_sfAllocInfo[ulSfIdx].m_ulSlotAllocInfo.size ();
				m_sfAllocInfo[ulSfIdx].m_ulSlotAllocInfo.push_back (slotInfo);
				m_sfAllocInfo[ulSfIdx].m_ulSlotAllocInfo.push_back (ulCtrlSlot);
=======
				// loop through TB info elements (can be multiple per DCI)
				SlotAllocInfo* slotInfo; // get reference to slot information
				SlotAllocInfo::TddMode slotMode;
				bool ulSlot = ((dciInfoElem.m_tddBitmap >> tbIt->m_slotInd) & 0x1);
				if (ulSlot)
				{
					slotMode = SlotAllocInfo::UL;
					//NS_ASSERT ((tbIt->m_slotInd % 2) == 1);
				}
				else
				{
					slotMode = SlotAllocInfo::DL;
				}
				if (slotMode == SlotAllocInfo::DL)
				{
					// Downlink TbInfoElement applies to current subframe
					slotInfo = &(m_currSfAllocInfo.m_slotAllocInfo[tbIt->m_slotInd]); // DL res alloc info, applies to this subframe
					if (m_currSfAllocInfo.m_tddPattern[tbIt->m_slotInd] == SlotAllocInfo::NA)
					{
						m_currSfAllocInfo.m_tddPattern[tbIt->m_slotInd] = SlotAllocInfo::DL;
					}
					else if (m_currSfAllocInfo.m_tddPattern[tbIt->m_slotInd] == SlotAllocInfo::UL)
					{
						NS_LOG_ERROR ("Slot already assigned in DL");
					}
					TbAllocInfo tbAllocInfo;
					tbAllocInfo.m_rnti = dciInfoElem.m_rnti;
					tbAllocInfo.m_tbInfo = *tbIt;
					for (unsigned irbg = 0; irbg < m_numRbg; irbg++) // assumes res alloc type 0
					{
						if((tbIt->m_rbBitmap >> irbg) & 0x1)
						{
							for (unsigned irb = 0; irb < m_phyMacConfig->GetNumRbPerRbg (); irb++)
							{
								// add all RBs for allocated RBGs to rbMap
								tbAllocInfo.m_rbMap.push_back (irbg * m_phyMacConfig->GetNumRbPerRbg () + irb);
							}
						}
					}
					slotInfo->m_tbInfo.push_back (tbAllocInfo);
				}
				else if (slotMode == SlotAllocInfo::UL)
				{
					// Uplink TbInfoElement applies to n+3th subframe
					TbAllocInfo tbAllocInfo;
					tbAllocInfo.m_rnti = dciInfoElem.m_rnti;
					tbAllocInfo.m_tbInfo = *tbIt;
					tbAllocInfo.m_sfnSf.m_frameNum = dciFrame;
					unsigned ulSfNum = dciSf + m_phyMacConfig->GetUlSchedDelay () - 1;
					if (ulSfNum > 10)
					{
						ulSfNum = ulSfNum % 10;
					}
					tbAllocInfo.m_sfnSf.m_subframeNum = ulSfNum;
					tbAllocInfo.m_sfnSf.m_slotNum = tbIt->m_slotInd+1;
					for (unsigned irb = 0; irb < tbAllocInfo.m_tbInfo.m_rbLen; irb++) // assumes res alloc type 0
					{
						tbAllocInfo.m_rbMap.push_back (tbAllocInfo.m_tbInfo.m_rbStart + irb);
					}
					QueueUlTbAlloc (tbAllocInfo);
				}
>>>>>>> b36c54152804fd16ccc9464c4123d6ceeba3fb48
			}

			m_phySapUser->ReceiveControlMessage (msg);
		}
		else if (msg->GetMessageType () == MmWaveControlMessage::MIB)
		{
			NS_LOG_INFO ("received MIB");
			NS_ASSERT (m_cellId > 0);
			Ptr<MmWaveMibMessage> msg2 = DynamicCast<MmWaveMibMessage> (msg);
			m_ueCphySapUser->RecvMasterInformationBlock (m_cellId, msg2->GetMib ());
		}
		else if (msg->GetMessageType () == MmWaveControlMessage::SIB1)
		{
			NS_ASSERT (m_cellId > 0);
			Ptr<MmWaveSib1Message> msg2 = DynamicCast<MmWaveSib1Message> (msg);
			m_ueCphySapUser->RecvSystemInformationBlockType1 (m_cellId, msg2->GetSib1 ());
		}
		else if (msg->GetMessageType () == MmWaveControlMessage::RAR)
		{
			NS_LOG_INFO ("received RAR");
			NS_ASSERT (m_cellId > 0);

			Ptr<MmWaveRarMessage> rarMsg = DynamicCast<MmWaveRarMessage> (msg);

			for (std::list<MmWaveRarMessage::Rar>::const_iterator it = rarMsg->RarListBegin ();
					it != rarMsg->RarListEnd ();
					++it)
			{
				if (it->rapId == m_raPreambleId)
				{
					m_phySapUser->ReceiveControlMessage (rarMsg);
				}
			}
		}
		else
		{
			NS_LOG_DEBUG ("Control message not handled. Type: "<< msg->GetMessageType());
		}
	}
}

void
MmWaveUePhy::QueueUlTbAlloc (TbAllocInfo m)
{
  NS_LOG_FUNCTION (this);
//  NS_LOG_DEBUG ("UL TB Info Elem queue size == " << m_ulTbAllocQueue.size ());
  m_ulTbAllocQueue.at (m_phyMacConfig->GetUlSchedDelay ()-1).push_back (m);
}

std::list<TbAllocInfo>
MmWaveUePhy::DequeueUlTbAlloc (void)
{
	NS_LOG_FUNCTION (this);

	if (m_ulTbAllocQueue.empty())
	{
		std::list<TbAllocInfo> emptylist;
		return (emptylist);
	}

	if (m_ulTbAllocQueue.at (0).size () > 0)
	{
		std::list<TbAllocInfo> ret = m_ulTbAllocQueue.at (0);
		m_ulTbAllocQueue.erase (m_ulTbAllocQueue.begin ());
		std::list<TbAllocInfo> l;
		m_ulTbAllocQueue.push_back (l);
		return (ret);
	}
	else
	{
		m_ulTbAllocQueue.erase (m_ulTbAllocQueue.begin ());
		std::list<TbAllocInfo> l;
		m_ulTbAllocQueue.push_back (l);
		std::list<TbAllocInfo> emptylist;
		return (emptylist);
	}
}

void
MmWaveUePhy::SubframeIndication (uint16_t frameNum, uint8_t sfNum)
{
	m_frameNum = frameNum;
	m_sfNum = sfNum;
	m_lastSfStart = Simulator::Now();
	m_currSfAllocInfo = m_sfAllocInfo[m_sfNum];
	NS_ASSERT ((m_currSfAllocInfo.m_sfnSf.m_frameNum == m_frameNum) &&
	           (m_currSfAllocInfo.m_sfnSf.m_sfNum == m_sfNum));
	m_sfAllocInfo[m_sfNum] = SfAllocInfo (SfnSf (m_frameNum+1, m_sfNum, 0));
	SlotAllocInfo dlCtrlSlot;
	dlCtrlSlot.m_numCtrlSym = 1;
	dlCtrlSlot.m_tddMode = SlotAllocInfo::DL;
	dlCtrlSlot.m_dci.m_numSym = 1;
	dlCtrlSlot.m_dci.m_symStart = 0;
	SlotAllocInfo ulCtrlSlot;
	ulCtrlSlot.m_numCtrlSym = 1;
	ulCtrlSlot.m_tddMode = SlotAllocInfo::UL;
	ulCtrlSlot.m_slotIdx = 0xFF;
	ulCtrlSlot.m_dci.m_numSym = 1;
	ulCtrlSlot.m_dci.m_symStart = m_phyMacConfig->GetSymbolsPerSubframe()-1;
	m_sfAllocInfo[m_sfNum].m_dlSlotAllocInfo.push_back (dlCtrlSlot);
	m_sfAllocInfo[m_sfNum].m_ulSlotAllocInfo.push_back (ulCtrlSlot);

	StartSlot ();
}

void
MmWaveUePhy::StartSlot ()
{
	unsigned slotInd = 0;
	SlotAllocInfo currSlot;
	if (m_slotNum >= m_currSfAllocInfo.m_dlSlotAllocInfo.size ())
	{
<<<<<<< HEAD
		if (m_currSfAllocInfo.m_ulSlotAllocInfo.size () > 0)
		{
			slotInd = m_slotNum - m_currSfAllocInfo.m_dlSlotAllocInfo.size ();
			currSlot = m_currSfAllocInfo.m_ulSlotAllocInfo[slotInd];
		}
=======
		// delay for control period reception/processing
		NS_LOG_DEBUG ("UE " << m_rnti << " RXing CTRL period frame " << m_nrFrames << " subframe " << sfInd << " slot " << slotInd << \
		              " start " << Simulator::Now() << " end " << Simulator::Now()+m_dlCtrlPeriod);
		Simulator::Schedule (m_dlCtrlPeriod, &MmWaveUePhy::ProcessSubframe, this);
>>>>>>> b36c54152804fd16ccc9464c4123d6ceeba3fb48
	}
	else
	{
		if (m_currSfAllocInfo.m_ulSlotAllocInfo.size () > 0)
		{
			slotInd = m_slotNum;
			currSlot = m_currSfAllocInfo.m_dlSlotAllocInfo[slotInd];
		}
	}

	m_currSlot = currSlot;

	NS_LOG_INFO ("UE " << m_rnti << " frame " << m_frameNum << " subframe " << m_sfNum << " slot " << m_slotNum);

<<<<<<< HEAD
	Time slotPeriod;
=======
	NS_LOG_INFO ("UE " << m_rnti << " frame " << m_nrFrames << " subframe " << sfInd << " slot " << slotInd);
>>>>>>> b36c54152804fd16ccc9464c4123d6ceeba3fb48

	if (m_slotNum == 0)  // reserved DL control
	{
		slotPeriod = NanoSeconds (1000.0 * m_phyMacConfig->GetSymbolPeriod () * m_phyMacConfig->GetDlCtrlSymbols ());
		NS_LOG_DEBUG ("UE" << m_rnti << " RXing DL CTRL frame " << m_frameNum << " subframe " << (unsigned)m_sfNum << " symbols "
		              << (unsigned)currSlot.m_dci.m_symStart << "-" << (unsigned)(currSlot.m_dci.m_symStart+currSlot.m_dci.m_numSym-1) <<
				              "\t start " << Simulator::Now() << " end " << (Simulator::Now()+slotPeriod));
	}
	else if (m_slotNum == m_currSfAllocInfo.m_dlSlotAllocInfo.size()+m_currSfAllocInfo.m_ulSlotAllocInfo.size()-1) // reserved UL control
	{
<<<<<<< HEAD
		m_receptionEnabled = false;
		SetSubChannelsForTransmission (m_channelChunks);
		slotPeriod = NanoSeconds (1000.0 * m_phyMacConfig->GetSymbolPeriod () * m_phyMacConfig->GetUlCtrlSymbols ());
		std::list<Ptr<MmWaveControlMessage> > ctrlMsg = GetControlMessages ();
		NS_LOG_DEBUG ("UE" << m_rnti << " TXing UL CTRL frame " << m_frameNum << " subframe " << (unsigned)m_sfNum << " symbols "
		              << (unsigned)currSlot.m_dci.m_symStart << "-" << (unsigned)(currSlot.m_dci.m_symStart+currSlot.m_dci.m_numSym-1) <<
			              "\t start " << Simulator::Now() << " end " << (Simulator::Now()+slotPeriod-NanoSeconds(1.0)));
		SendCtrlChannels (ctrlMsg, slotPeriod-NanoSeconds(1.0));
=======
		// process UL grants previously received
		std::list<TbAllocInfo> ulAllocInfoList = DequeueUlTbAlloc ();
		std::list<TbAllocInfo>::iterator it = ulAllocInfoList.begin ();
		if(it == ulAllocInfoList.end ())
		{
			m_ulGrant = false;	// no grant received, need to transmit UL control on PUCCH
		}
		else
		{
			m_ulGrant = true;
		}
		NS_LOG_DEBUG ("eNB Expected UL TBs " << ulAllocInfoList.size () << " from UE " << m_rnti);
		while(it != ulAllocInfoList.end ())
		{
			NS_ASSERT(it->m_sfnSf.m_subframeNum == sfInd);
			NS_ASSERT(m_currSfAllocInfo.m_slotAllocInfo.size ()>0);
			SlotAllocInfo& slotInfo = m_currSfAllocInfo.m_slotAllocInfo[it->m_tbInfo.m_slotInd];
			//NS_ASSERT(slotInfo.m_tbInfo.size()==0);
			slotInfo.m_tbInfo.push_back (*it);
			if (m_currSfAllocInfo.m_tddPattern[it->m_tbInfo.m_slotInd] == SlotAllocInfo::NA)
			{
				m_currSfAllocInfo.m_tddPattern[it->m_tbInfo.m_slotInd] = SlotAllocInfo::UL;
				slotInfo.m_tddMode = SlotAllocInfo::UL;
			}
			else if (m_currSfAllocInfo.m_tddPattern[it->m_tbInfo.m_slotInd] == SlotAllocInfo::DL)
			{
				NS_LOG_ERROR ("Slot already assigned in DL");
			}
			NS_LOG_INFO ("UE: UL slot " << (unsigned) slotInfo.m_slotInd);
			it++;
		}
		slotEnd = Seconds(GetTti()) - m_dlCtrlPeriod;
>>>>>>> b36c54152804fd16ccc9464c4123d6ceeba3fb48
	}
	else if (currSlot.m_dci.m_format == DciInfoElementTdma::DL)  // scheduled DL data slot
	{
		m_receptionEnabled = true;
		slotPeriod = NanoSeconds (1000.0 * m_phyMacConfig->GetSymbolPeriod () * currSlot.m_dci.m_numSym);
		m_downlinkSpectrumPhy->AddExpectedTb (currSlot.m_dci.m_rnti, currSlot.m_dci.m_ndi, currSlot.m_dci.m_tbSize, currSlot.m_dci.m_mcs,
		                                      m_channelChunks, currSlot.m_dci.m_harqProcess, currSlot.m_dci.m_rv, true);
		m_reportDlTbSize (GetDevice ()->GetObject <MmWaveUeNetDevice> ()->GetImsi(), currSlot.m_dci.m_tbSize);
		NS_LOG_DEBUG ("UE" << m_rnti << " RXing DL DATA frame " << m_frameNum << " subframe " << (unsigned)m_sfNum << " symbols "
		              << (unsigned)currSlot.m_dci.m_symStart << "-" << (unsigned)(currSlot.m_dci.m_symStart+currSlot.m_dci.m_numSym-1) <<
		              "\t start " << Simulator::Now() << " end " << (Simulator::Now()+slotPeriod));
	}
	else if (currSlot.m_dci.m_format == DciInfoElementTdma::UL) // scheduled UL data slot
	{
		m_receptionEnabled = false;
		SetSubChannelsForTransmission (m_channelChunks);
		slotPeriod = NanoSeconds (1000.0 * m_phyMacConfig->GetSymbolPeriod () * currSlot.m_dci.m_numSym);
		std::list<Ptr<MmWaveControlMessage> > ctrlMsg = GetControlMessages ();
		Ptr<PacketBurst> pktBurst = GetPacketBurst (SfnSf(m_frameNum, m_sfNum, currSlot.m_dci.m_symStart));
		std::list< Ptr<Packet> > pkts = pktBurst->GetPackets ();
		if (!pkts.empty ())
		{
			MmWaveMacPduTag tag;
			pkts.front ()->PeekPacketTag (tag);
			NS_ASSERT ((tag.GetSfn().m_sfNum == m_sfNum) && (tag.GetSfn().m_slotNum == currSlot.m_dci.m_symStart));

			LteRadioBearerTag bearerTag;
			if(!pkts.front ()->PeekPacketTag (bearerTag))
			{
				NS_FATAL_ERROR ("No radio bearer tag");
			}
		}
		m_reportUlTbSize (GetDevice ()->GetObject <MmWaveUeNetDevice> ()->GetImsi(), currSlot.m_dci.m_tbSize);
		NS_LOG_DEBUG ("UE" << m_rnti << " TXing UL DATA frame " << m_frameNum << " subframe " << (unsigned)m_sfNum << " symbols "
		              << (unsigned)currSlot.m_dci.m_symStart << "-" << (unsigned)(currSlot.m_dci.m_symStart+currSlot.m_dci.m_numSym-1)
		              << "\t start " << Simulator::Now() << " end " << (Simulator::Now()+slotPeriod));
		Simulator::ScheduleNow(&MmWaveUePhy::SendDataChannels, this, pktBurst, ctrlMsg, slotPeriod-NanoSeconds(2.0), m_slotNum);
	}

	m_prevSlotDir = currSlot.m_tddMode;

	m_phySapUser->SubframeIndication (SfnSf(m_frameNum, m_sfNum, m_slotNum)); 	// trigger mac

	//NS_LOG_DEBUG ("MmWaveUePhy: Scheduling slot end for " << slotPeriod);
	Simulator::Schedule (slotPeriod, &MmWaveUePhy::EndSlot, this);
}


void
MmWaveUePhy::EndSlot ()
{
	if (m_slotNum == m_currSfAllocInfo.m_dlSlotAllocInfo.size()+m_currSfAllocInfo.m_ulSlotAllocInfo.size()-1)
	{	// end of subframe
		uint16_t frameNum;
		uint8_t sfNum;
		if (m_sfNum == m_phyMacConfig->GetSubframesPerFrame ()-1)
		{
			sfNum = 0;
//			if (m_frameNum == 1023)
//			{
//				frameNum = 0;
//			}
<<<<<<< HEAD
//			else
//			{
//				frameNum = m_frameNum + 1;
//			}
			frameNum = m_frameNum + 1;
		}
		else
		{
			frameNum = m_frameNum;
			sfNum = m_sfNum + 1;
		}
		m_slotNum = 0;
		//NS_LOG_DEBUG ("MmWaveUePhy: Next subframe scheduled for " << m_lastSfStart + m_sfPeriod - Simulator::Now());
		Simulator::Schedule (m_lastSfStart + m_sfPeriod - Simulator::Now(), &MmWaveUePhy::SubframeIndication, this, frameNum, sfNum);
=======
//		}
		SetSubChannelsForTransmission (ulRbChunks);
		// assume for now that the PUCCH is transmitted over one OFDM symbol across all RBs
//		NS_LOG_DEBUG ("UE " << m_rnti << " TXing PUCCH (num ctrl msgs == " << ctrlMsg.size () << ")");
		NS_LOG_DEBUG ("UE " << m_rnti << " TXing CTRL period frame " << m_nrFrames << " subframe " << sfInd << " slot " << slotInd << \
		              " start " << Simulator::Now() << " end " << (Simulator::Now()+m_ulCtrlPeriod));
		MmWaveUePhy::SendCtrlChannels (ctrlMsg, m_ulCtrlPeriod);
		m_prevSlotDir = SlotAllocInfo::UL;
		ctrlGuardPeriod += m_ulCtrlPeriod;
>>>>>>> b36c54152804fd16ccc9464c4123d6ceeba3fb48
	}
	else
	{
		Time nextSlotStart;
		uint8_t slotInd = m_slotNum+1;
		if (slotInd >= m_currSfAllocInfo.m_dlSlotAllocInfo.size ())
		{
			if (m_currSfAllocInfo.m_ulSlotAllocInfo.size () > 0)
			{
<<<<<<< HEAD
				slotInd = slotInd - m_currSfAllocInfo.m_dlSlotAllocInfo.size ();
				nextSlotStart = NanoSeconds (1000.0 * m_phyMacConfig->GetSymbolPeriod () *
				                             m_currSfAllocInfo.m_ulSlotAllocInfo[slotInd].m_dci.m_symStart);
=======
				std::vector <int> dlRbChunks;
				for (unsigned itb = 0; itb < slotInfo.m_tbInfo.size (); itb++)
				{
					TbAllocInfo& tbAlloc = slotInfo.m_tbInfo[itb];
					// translate the TB info to Spectrum framework
					for (unsigned irb = 0; irb < tbAlloc.m_rbMap.size (); irb++)
					{
						unsigned rbInd = tbAlloc.m_rbMap[irb];
						for(unsigned ichunk = 0; ichunk < m_phyMacConfig->GetNumChunkPerRb (); ichunk++)
						{
							dlRbChunks.push_back (rbInd * m_phyMacConfig->GetNumChunkPerRb () + ichunk);
							// NS_LOG_DEBUG(this << " RNTI " << m_rnti << " RBG " << i << " DL-DCI allocated PRB " << (i*GetRbgSize()) + k);
						}
					}
					SetSubChannelsForReception (dlRbChunks);

					// send TB info to LteSpectrumPhy
					NS_LOG_DEBUG ("UE " << m_rnti << " DL-DCI " << tbAlloc.m_rnti << " bitmap "  << tbAlloc.m_tbInfo.m_rbBitmap);
					NS_LOG_DEBUG ("UE " << m_rnti << " RXing DATA period frame " << m_nrFrames << " subframe " << sfInd << " slot " << slotInd << \
											              " start " << Simulator::Now() << " end " << (Simulator::Now()+slotEnd));
					m_downlinkSpectrumPhy->AddExpectedTb (tbAlloc.m_rnti, tbAlloc.m_tbInfo.m_tbSize, tbAlloc.m_tbInfo.m_mcs, dlRbChunks, true);
					m_reportDlTbSize (GetDevice ()->GetObject <MmWaveUeNetDevice> ()->GetImsi(), tbAlloc.m_tbInfo.m_tbSize);
				}
>>>>>>> b36c54152804fd16ccc9464c4123d6ceeba3fb48
			}
		}
		else
		{
			if (m_currSfAllocInfo.m_ulSlotAllocInfo.size () > 0)
			{
<<<<<<< HEAD
				nextSlotStart = NanoSeconds (1000.0 * m_phyMacConfig->GetSymbolPeriod () *
				                             m_currSfAllocInfo.m_dlSlotAllocInfo[slotInd].m_dci.m_symStart);
			}
		}
		m_slotNum++;
		Simulator::Schedule (nextSlotStart+m_lastSfStart-Simulator::Now(), &MmWaveUePhy::StartSlot, this);
=======
				std::vector <int> ulRbChunks;
				for (unsigned itb = 0; itb < slotInfo.m_tbInfo.size (); itb++)
				{
					TbAllocInfo& tbAlloc = slotInfo.m_tbInfo[itb];
					NS_ASSERT(itb==0);

					// translate the TB info to Spectrum framework
					for (unsigned irb = 0; irb < tbAlloc.m_rbMap.size (); irb++)
					{
						unsigned rbInd = tbAlloc.m_rbMap[irb];
						for(unsigned ichunk = 0; ichunk < m_phyMacConfig->GetNumChunkPerRb (); ichunk++)
						{
							ulRbChunks.push_back (rbInd * m_phyMacConfig->GetNumChunkPerRb () + ichunk);
							// NS_LOG_DEBUG(this << " RNTI " << m_rnti << " RBG " << i << " DL-DCI allocated PRB " << (i*GetRbgSize()) + k);
						}
					}
					SetSubChannelsForTransmission (ulRbChunks);

					std::list<Ptr<MmWaveControlMessage> > ctrlMsg = GetControlMessages ();

					Ptr<PacketBurst> pktBurst = GetPacketBurst (sfInd, slotInd);
					std::list< Ptr<Packet> > pkts = pktBurst->GetPackets ();
					if (!pkts.empty ())
					{
						MmWaveMacPduTag tag;
						pkts.front ()->PeekPacketTag (tag);
						NS_ASSERT ((tag.GetSubframeNum() == sfInd) && (tag.GetSlotNum() == slotInd));
					}

					if (0 && slotDir == SlotAllocInfo::DL && m_prevSlotDir == SlotAllocInfo::UL)  // if curr slot == DL and prev slot == UL
					{
						ctrlGuardPeriod += NanoSeconds (1000 * m_phyMacConfig->GetGuardPeriod ());
					}
					else
					{
						ctrlGuardPeriod += Seconds(0.0);
					}

					Time dataPeriod = Seconds (m_phyMacConfig->GetTti()) - ctrlGuardPeriod;

					NS_LOG_DEBUG ("UE " << m_rnti << " UL-DCI " << tbAlloc.m_rnti << " rbStart "  << (unsigned)tbAlloc.m_tbInfo.m_rbStart << " rbLen "  << (unsigned)tbAlloc.m_tbInfo.m_rbLen);
										NS_LOG_DEBUG ("UE " << m_rnti << " TXing DATA period frame " << m_nrFrames << " subframe " << sfInd << " slot " << slotInd << \
																              " start " << Simulator::Now()+ctrlGuardPeriod+NanoSeconds(2.0) << " end " << Simulator::Now()+ctrlGuardPeriod+dataPeriod-NanoSeconds(1.0));
					Simulator::Schedule (ctrlGuardPeriod+NanoSeconds(1.0), &MmWaveUePhy::SendDataChannels, this, pktBurst, ctrlMsg, dataPeriod-NanoSeconds(2.0), slotInd);
					m_reportUlTbSize (GetDevice ()->GetObject <MmWaveUeNetDevice> ()->GetImsi(), tbAlloc.m_tbInfo.m_tbSize);
				}
			}
		}
		m_prevSlotDir = slotDir;


	m_phySapUser->SubframeIndication (m_nrFrames, sfInd, slotInd); 	/*triger mac*/

	if (m_nrSlots == (m_phyMacConfig->GetSlotsPerSubframe () * m_phyMacConfig->GetSubframesPerFrame ()))
	{
		m_nrSlots = 1;
		m_nrFrames++;
		if (m_nrFrames == 1024)
		{
			m_nrFrames = 0;
		}
	}
	else
	{
		m_nrSlots++;
	}

	if (slotInd == 8)
	{
		m_currSfAllocInfo = SfAllocationInfo (m_phyMacConfig->GetSlotsPerSubframe ());
>>>>>>> b36c54152804fd16ccc9464c4123d6ceeba3fb48
	}
}


uint32_t
MmWaveUePhy::GetSubframeNumber (void)
{
	return m_slotNum;
}

void
MmWaveUePhy::PhyDataPacketReceived (Ptr<Packet> p)
{
	m_phySapUser->ReceivePhyPdu (p);
}

void
MmWaveUePhy::SendDataChannels (Ptr<PacketBurst> pb, std::list<Ptr<MmWaveControlMessage> > ctrlMsg, Time duration, uint8_t slotInd)
{

	//Ptr<AntennaArrayModel> antennaArray = DynamicCast<AntennaArrayModel> (GetDlSpectrumPhy ()->GetRxAntenna());
	/* set beamforming vector;
	 * for UE, you can choose 16 antenna with 0-7 sectors, or 4 antenna with 0-3 sectors
	 * input is (sector, antenna number)
	 *
	 * */
	//antennaArray->SetSector (3,16);

	if (pb->GetNPackets() > 0)
	{
		LteRadioBearerTag tag;
		if(!pb->GetPackets().front()->PeekPacketTag (tag))
		{
			NS_FATAL_ERROR ("No radio bearer tag");
		}
	}

	m_downlinkSpectrumPhy->StartTxDataFrames (pb, ctrlMsg, duration, slotInd);
}

void
MmWaveUePhy::SendCtrlChannels (std::list<Ptr<MmWaveControlMessage> > ctrlMsg, Time prd)
{
	m_downlinkSpectrumPhy->StartTxDlControlFrames(ctrlMsg,prd);
}


uint32_t
MmWaveUePhy::GetAbsoluteSubframeNo ()
{
	return ((m_frameNum-1)*8 + m_slotNum);
}

Ptr<MmWaveDlCqiMessage>
MmWaveUePhy::CreateDlCqiFeedbackMessage (const SpectrumValue& sinr)
{
	if (!m_amc)
	{
		m_amc = CreateObject <MmWaveAmc> (m_phyMacConfig);
	}
	NS_LOG_FUNCTION (this);
	SpectrumValue newSinr = sinr;
	// CREATE DlCqiLteControlMessage
	Ptr<MmWaveDlCqiMessage> msg = Create<MmWaveDlCqiMessage> ();
	DlCqiInfo dlcqi;

	dlcqi.m_rnti = m_rnti;
	dlcqi.m_cqiType = DlCqiInfo::WB;

	std::vector<int> cqi;

	//uint8_t dlBandwidth = m_phyMacConfig->GetNumChunkPerRb () * m_phyMacConfig->GetNumRb ();
	NS_ASSERT (m_currSlot.m_dci.m_format==0);
	int mcs;
	dlcqi.m_wbCqi = m_amc->CreateCqiFeedbackWbTdma (newSinr, m_currSlot.m_dci.m_numSym, m_currSlot.m_dci.m_tbSize, mcs);

//	int activeSubChannels = newSinr.GetSpectrumModel()->GetNumBands ();
	/*cqi = m_amc->CreateCqiFeedbacksTdma (newSinr, m_currNumSym);
	int nbSubChannels = cqi.size ();
	double cqiSum = 0.0;
	// average the CQIs of the different RBs
	for (int i = 0; i < nbSubChannels; i++)
	{
		if (cqi.at (i) != -1)
		{
			cqiSum += cqi.at (i);
			activeSubChannels++;
		}
//		NS_LOG_DEBUG (this << " subch " << i << " cqi " <<  cqi.at (i));
	}*/
//	if (activeSubChannels > 0)
//	{
//		dlcqi.m_wbCqi = ((uint16_t) cqiSum / activeSubChannels);
//	}
//	else
//	{
//		// approximate with the worst case -> CQI = 1
//		dlcqi.m_wbCqi = 1;
//	}
	msg->SetDlCqi (dlcqi);
	return msg;
}

void
MmWaveUePhy::GenerateDlCqiReport (const SpectrumValue& sinr)
{
	if(m_ulConfigured && (m_rnti > 0))
	{
		if (Simulator::Now () > m_wbCqiLast + m_wbCqiPeriod)
		{
			SpectrumValue newSinr = sinr;
			Ptr<MmWaveDlCqiMessage> msg = CreateDlCqiFeedbackMessage (newSinr);

			if (msg)
			{
				DoSendControlMessage (msg);
			}
			Ptr<MmWaveUeNetDevice> UeRx = DynamicCast<MmWaveUeNetDevice> (GetDevice());
			m_reportCurrentCellRsrpSinrTrace (UeRx->GetImsi(), newSinr, newSinr);
		}
	}
}

void
MmWaveUePhy::ReceiveLteDlHarqFeedback (DlHarqInfo m)
{
  NS_LOG_FUNCTION (this);
  // generate feedback to eNB and send it through ideal PUCCH
  Ptr<MmWaveDlHarqFeedbackMessage> msg = Create<MmWaveDlHarqFeedbackMessage> ();
  msg->SetDlHarqFeedback (m);
  DoSendControlMessage (msg);
}

bool
MmWaveUePhy::IsReceptionEnabled ()
{
	return m_receptionEnabled;
}

void
MmWaveUePhy::ResetReception()
{
	m_receptionEnabled = false;
}

uint16_t
MmWaveUePhy::GetRnti ()
{
	return m_rnti;
}


void
MmWaveUePhy::DoReset ()
{
	NS_LOG_FUNCTION (this);
}

void
MmWaveUePhy::DoStartCellSearch (uint16_t dlEarfcn)
{
	NS_LOG_FUNCTION (this << dlEarfcn);
}

void
MmWaveUePhy::DoSynchronizeWithEnb (uint16_t cellId, uint16_t dlEarfcn)
{
	NS_LOG_FUNCTION (this << cellId << dlEarfcn);
	DoSynchronizeWithEnb (cellId);
}

void
MmWaveUePhy::DoSynchronizeWithEnb (uint16_t cellId)
{
	NS_LOG_FUNCTION (this << cellId);
	if (cellId == 0)
	{
		NS_FATAL_ERROR ("Cell ID shall not be zero");
	}
	/*
	m_cellId = cellId;
	//TBD how to assign bandwitdh and earfcn
	m_noiseFigure = 5.0;
	//m_Bandwidth = BANDWIDTH;
	//m_earfcn = 1;

	Ptr<SpectrumValue> noisePsd =
			MmWaveSpectrumValueHelper::CreateNoisePowerSpectralDensity (m_earfcn, m_Bandwidth, m_noiseFigure);
	m_downlinkSpectrumPhy->SetNoisePowerSpectralDensity (noisePsd);
	m_downlinkSpectrumPhy->GetSpectrumChannel()->AddRx(m_downlinkSpectrumPhy);
	m_downlinkSpectrumPhy->SetCellId(m_cellId);*/
}

void
MmWaveUePhy::DoSetPa (double pa)
{
  NS_LOG_FUNCTION (this << pa);
}

void
MmWaveUePhy::DoSetDlBandwidth (uint8_t dlBandwidth)
{
	NS_LOG_FUNCTION (this << (uint32_t) dlBandwidth);
}


void
MmWaveUePhy::DoConfigureUplink (uint16_t ulEarfcn, uint8_t ulBandwidth)
{
	NS_LOG_FUNCTION (this << ulEarfcn << ulBandwidth);
  m_ulConfigured = true;
}

void
MmWaveUePhy::DoConfigureReferenceSignalPower (int8_t referenceSignalPower)
{
	NS_LOG_FUNCTION (this << referenceSignalPower);
}

void
MmWaveUePhy::DoSetRnti (uint16_t rnti)
{
	NS_LOG_FUNCTION (this << rnti);
	m_rnti = rnti;
}

void
MmWaveUePhy::DoSetTransmissionMode (uint8_t txMode)
{
	NS_LOG_FUNCTION (this << (uint16_t)txMode);
}

void
MmWaveUePhy::DoSetSrsConfigurationIndex (uint16_t srcCi)
{
	NS_LOG_FUNCTION (this << srcCi);
}

void
MmWaveUePhy::SetPhySapUser (MmWaveUePhySapUser* ptr)
{
	m_phySapUser = ptr;
}

void
MmWaveUePhy::SetHarqPhyModule (Ptr<MmWaveHarqPhy> harq)
{
  m_harqPhyModule = harq;
}

}


