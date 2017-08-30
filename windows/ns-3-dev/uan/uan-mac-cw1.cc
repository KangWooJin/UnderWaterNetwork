/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Leonard Tracy <lentracy@gmail.com>
 */

/*
*
*
*
*
*
*/
#include "uan-mac-cw1.h"
#include "ns3/attribute.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/nstime.h"
#include "ns3/uan-header-common.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/log.h"


NS_LOG_COMPONENT_DEFINE ("UanMacCw1");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (UanMacCw1);

UanMacCw1::UanMacCw1 ()
  : UanMac (),
    m_phy (0),
    m_pktTx (0),
    m_state (IDLE),
	m_cleared (false),
	m_currentRate(1500),
	m_maxPropDelay(1.4142),
	m_RetryLimit(3),
	m_retryCnt(0),
	m_failCount(0),
	m_successCount(0),
	m_mifs(0.001),
	m_navTime(Seconds(0)),
	m_difsTime(Seconds(0)),
	m_sendTime(Seconds(0)),
    m_backoffTime(Seconds(0)),
    m_sifsTime (Seconds(0)),
	m_sleepTime(Seconds(0)),
	m_reset_cw(10), // ���� 10
	dataBytes(200), // ���� 200
	m_sifs(0.2), // ���� 0.2
	m_difs(2*1.5+0.2), // ���� 2*slottime + sifs;
	m_ACK( 17 + 14 + 3 ),
	m_RTS( 17 + 17 + 3 ),
	m_CTS( 17 + 11 + 3 ),
	m_DATA(200+17+30),
	//37 + 31 + 247 + 34
	m_SumDelays(Seconds(0)),
	m_pointFlag(true),
	m_nodeMAX(255),
	APNode(9999),
	m_waitFlag(false),
	m_exceptCount(0)
	

{
  m_rv = CreateObject<UniformRandomVariable> ();
}

UanMacCw1::~UanMacCw1 ()
{
}

void
UanMacCw1::Clear ()
{
  if (m_cleared)
    {
      return;
    }
  m_cleared = true;
  m_pktTx.clear();
  m_pktTxProt.clear();
  m_transPktTx = 0 ;
  m_propDelay.clear();
  m_distance.clear();
  m_sortedDistance.clear();

  if (m_phy)
    {
      m_phy->Clear ();
      m_phy = 0;
    }
  m_sendEvent.Cancel ();
  m_txEndEvent.Cancel ();
  m_difsEvent.Cancel();
  m_backOffEvent.Cancel();
  m_timerEvent.Cancel();
  m_navEndEvent.Cancel();
  m_sifsEvent.Cancel();
  m_eifsEvent.Cancel();
}

void
UanMacCw1::DoDispose ()
{
  Clear ();
  UanMac::DoDispose ();
  
}

/*Attribute�� �����ϸ� main�� ���� ������ �� ����*/
TypeId
UanMacCw1::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanMacCw1")
    .SetParent<Object> ()
    .AddConstructor<UanMacCw1> ()
	.AddAttribute("NodeIncrease",
				  "NodeIncrease",
				  UintegerValue (2),
				  MakeUintegerAccessor (&UanMacCw1::m_nodeIncrease),
                  MakeUintegerChecker<uint32_t> ())
	.AddAttribute("CurrNodes",
				  "CurrNodes",
				  UintegerValue (2),
				  MakeUintegerAccessor (&UanMacCw1::m_currNodes),
                  MakeUintegerChecker<uint32_t> ())
	.AddAttribute("Boundary",
				  "boundary",
				  UintegerValue (1500),
				  MakeUintegerAccessor (&UanMacCw1::m_boundary),
                  MakeUintegerChecker<uint32_t> ())
	.AddAttribute("DataRate",
				  "1500m/s",
				  UintegerValue (1500),
				  MakeUintegerAccessor (&UanMacCw1::m_currentRate),
                  MakeUintegerChecker<uint32_t> ())
	.AddAttribute("Point",
				  "Node x,y",
				  StringValue(""),
				  MakeStringAccessor(&UanMacCw1::m_point),
				  MakeStringChecker())
    .AddAttribute ("CW",
                   "The MAC parameter CW",
				   UintegerValue (10),
                   MakeUintegerAccessor (&UanMacCw1::m_cw),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("SlotTime",
                   "Time slot duration for MAC backoff",
                   TimeValue (Seconds(1.5)),
                   MakeTimeAccessor (&UanMacCw1::m_slotTime),
                   MakeTimeChecker ())
	.AddAttribute ("QueueLimit",
                   "Maximum packets to queue at MAC",
                   UintegerValue (10),
				   MakeUintegerAccessor (&UanMacCw1::m_queueLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Enqueue",
                     "A packet arrived at the MAC for transmission",
                     MakeTraceSourceAccessor (&UanMacCw1::m_enqueueLogger))
    .AddTraceSource ("Dequeue",
                     "A was passed down to the PHY from the MAC",
                     MakeTraceSourceAccessor (&UanMacCw1::m_dequeueLogger))
    .AddTraceSource ("RX",
                     "A packet was destined for this MAC and was received",
                     MakeTraceSourceAccessor (&UanMacCw1::m_rxLogger))

  ;
  return tid;
}

Address
UanMacCw1::GetAddress ()
{
  return this->m_address;
}

void
UanMacCw1::SetAddress (UanAddress addr)
{
  m_address = addr;
}

bool
UanMacCw1::Enqueue (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
	if (protocolNumber > 0)
    {
      NS_LOG_WARN ("Warning: UanMacRc does not support multiple protocols.  protocolNumber argument to Enqueue is being ignored");
    }

	//��ǥ�� ����
	if (m_pointFlag)
	{
		setPoint();
	}

	if (m_pktTx.size () >= m_queueLimit)
    {
		/* ��Ŷ �����÷ο�*/
      return false;
    }

  //��Ŷ ���
	if ( DEBUG_PRINT > 2)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " packet size -> " << packet->GetSize() << '\n';

    m_pktTx.push_back (std::make_pair (packet, UanAddress::ConvertFrom (dest)));
	
    m_pktTxProt.push_back(protocolNumber);


  switch (m_state)
    {
   
    case RUNNING:
		if ( DEBUG_PRINT > 1)
		 cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " Starting enqueue RUNNING\n";
	  break;
    case TX:
		if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " Starting enqueue TX\n";
		break;
	case RX:
		if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " Starting enqueue RX\n";
		break;
	case BACKOFF:
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " Starting enqueue BACKOFF\n";
		break;
	case NAV:
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " Starting enqueue NAV\n";
		break;
	case RTSSENT:
	case DATATX:
		if ( DEBUG_PRINT >1)
			cout << "Time " << Simulator::Now ().GetSeconds () <<  " Addr " << (uint32_t)m_address.GetAsInt() << " Starting enqueue RTSSENT | DATATX\n";
		break;
	case EIFS:
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " Starting enqueue EIFS\n";
		break;
    case IDLE:
      {

		  //IDLE üũ
		if ( !isIDLE() )
		{
			//busy����
			return true;
		}
		//RUNNING ���� ��Ŷ�� �����غ��ϴ� �ܰ�  
		m_state = RUNNING;

		m_difsEvent = Simulator::Schedule ( Seconds(m_difs), &UanMacCw1::difsTimer, this); // 2*SlotTime(0.2)+ SIFS(0.2)
		m_difsTime = Seconds(m_difs)+Simulator::Now();
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": DIFS�̺�Ʈ ���� " << m_difsTime.GetSeconds() << "�� DIFS �̺�Ʈ �ߵ�\n";
        break;
      }
    default:
		if ( DEBUG_PRINT > 1)
			cout << " Addr " << (uint32_t)m_address.GetAsInt() << " Starting enqueue SOMETHING ELSE\n";
    }
  return true;
}

void
UanMacCw1::SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress&> cb)
{
  m_forwardUpCb = cb;
}

void
UanMacCw1::AttachPhy (Ptr<UanPhy> phy)
{
  m_phy = phy;
  m_phy->SetReceiveOkCallback (MakeCallback (&UanMacCw1::PhyRxPacketGood, this));
  m_phy->SetReceiveErrorCallback (MakeCallback (&UanMacCw1::PhyRxPacketError, this));
  m_phy->RegisterListener (this);
}

Address
UanMacCw1::GetBroadcast (void) const
{
  return UanAddress::GetBroadcast ();
}


void
UanMacCw1::NotifyRxStart (void)
{
	if ( DEBUG_PRINT > 1)
		cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " RxStart \n";
	
	//����� ���̸� ����ϰ� ���� Backoff�ð��� ����ص�
	if ( m_state == BACKOFF || m_backoffTime != Seconds(0) )
	{
		m_backoff = m_backoffTime - Simulator::Now();
		Simulator::Cancel(m_backOffEvent);
		m_backoffTime = Seconds(0);
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": BACKOFF �̺�Ʈ ���� BACKOFF �����ð� : " << m_backoff.GetSeconds() << "\n";
	}
	//���¸� RX�� ����
	m_prevState = m_state ;
	m_state = RX;
	//SIFS���̹Ƿ� RX����
	if ( m_sifsTime != Seconds(0))
	{
		m_state = RUNNING;
		if ( DEBUG_PRINT > 1)
			cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " SIFS �̺�Ʈ ���̹Ƿ� RxStart ���� \n";
	}
	//RX���� �̹Ƿ� DIFS �̺�Ʈ ����
	if ( m_difsTime != Seconds(0) )
	{
		if ( DEBUG_PRINT >1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� RX �����̹Ƿ� difs �̺�Ʈ ����\n";
		Simulator::Cancel(m_difsEvent);
		m_difsTime = Seconds(0);
	}
}
void
UanMacCw1::NotifyRxEndOk (void)
{
	if ( DEBUG_PRINT > 1)
		cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " RxEndOk \n";
	m_prevState = m_state ;
	//RX�� �������Ƿ� RUNNING
	m_state = RUNNING;
}
void
UanMacCw1::NotifyRxEndError (void)
{
	if ( DEBUG_PRINT > 1)
		cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " RxEndError \n";

	//EIFS�߿� �� �����ϴ°�� ����ϰ� �ٽ� ����
	if (m_eifsEvent.IsRunning ())
    {
		if ( DEBUG_PRINT > 1)
			cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " Eifs �ߵ� �� �ΰ� ���\n";
      Simulator::Cancel (m_eifsEvent);
    }

	m_prevState = m_state ;
	m_state = EIFS;

	m_eifsTime = Seconds( (m_ACK*8.0)/m_currentRate ) + Seconds(m_sifs) ; // ACK + PHY ���۽ð� + SIFS�ð� 
	m_eifsEvent = Simulator::Schedule (m_eifsTime, &UanMacCw1::eifsTimer, this);
	m_eifsTime = m_eifsTime + Simulator::Now() ;

	if ( DEBUG_PRINT > 1)
		cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " Eifs �̺�Ʈ ���� " << m_eifsTime.GetSeconds() << "�� Eifs �̺�Ʈ �ߵ�\n";

}
void
UanMacCw1::NotifyCcaStart (void)
{
	if ( DEBUG_PRINT > 2)
		cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " CcaStart \n";
}
void
UanMacCw1::NotifyCcaEnd (void)
{
	if ( DEBUG_PRINT >2)
		cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " CcaEnd \n";
}
void
UanMacCw1::NotifyTxStart (Time duration)
{
	/*sendPacket�� �ϸ� �ߵ��Ǵ� �Լ�*/
	if ( DEBUG_PRINT >1)
		cout <<  "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() <<  " Tx start " << " Time " <<  Simulator::Now ().GetSeconds () + duration.GetSeconds() << "�� Tx End\n" ;
  if (m_txEndEvent.IsRunning ())
    {
      Simulator::Cancel (m_txEndEvent);
    }

  m_prevState = m_state ;
  m_state = TX ;

  m_txEndEvent = Simulator::Schedule (duration, &UanMacCw1::EndTx, this);
  
}

int64_t
UanMacCw1::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rv->SetStream (stream);
  return 1;
}

void
UanMacCw1::EndTx (void)
{
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " Tx End\n" ;

	//TX ������ IDLE����
  if (m_state == TX)
    {
      m_state = IDLE;
    }
  
}
void
UanMacCw1::SetCw (uint32_t cw)
{
  m_cw = cw;
  m_reset_cw = cw ;
}
void
UanMacCw1::SetSlotTime (Time duration)
{
  m_slotTime = duration;
}
uint32_t
UanMacCw1::GetCw (void)
{
  return m_cw;
}
Time
UanMacCw1::GetSlotTime (void)
{
  return m_slotTime;
}
void
UanMacCw1::PhyRxPacketGood (Ptr<Packet> pkt, double sinr, UanTxMode mode)
{
	if ( DEBUG_PRINT > 1)
		cout <<  "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " RxEnd �����ð�\n";

	/*������������ �ø��ºκ�*/
	//����ó��
	if ( m_sifsTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout <<  "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " SIFS�߿� ��Ŷ�� ������ ��Ŷ ����\n";

		m_prevState = m_state;
		m_state = RUNNING;
		return ;
	}

	if ( m_state == RX)
	{
		if ( DEBUG_PRINT > 1)
			cout <<  "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " RX�߿� ��Ŷ�� ������ �� ����\n";
		NS_FATAL_ERROR ( "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " RX�߿� ��Ŷ�� ������ �� ����");
	}

	uint32_t bytes  = pkt->GetSize();

	//if ( DEBUG_PRINT > 1)
			//cout <<  "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " packet size->" << pkt->GetSize();
		// data(200) + phy(17) + data(27) + common(3) -> 247
		// phy(17) + rts(17) + common(3) -> 37
		// phy(17) + cts(11) + common(3) -> 31
		// phy(17) + ack(14) + common(3) -> 34


	UanHeaderCommon ch;
	pkt->RemoveHeader (ch);

	//1. RTS/CTS/DATA/ACK�� ����� �и��Ѵ�.
	
	UanHeaderRcData1 datah;
	UanHeaderRcRts1 rtsh;
	UanHeaderRcCts1 ctsh;
	UanHeaderRcAck1 ackh;
	UanHeaderRcPhy ph;
	Ptr<Packet> dataPkt ;

	//���� ��Ŷ�� ����� ������ ���
	switch(ch.GetType())
	{
	case TYPE_DATA:
		pkt->RemoveHeader(datah);
		pkt->RemoveHeader(ph);

		if ( ch.GetDest() == m_address)
		{
			dataPkt = pkt ; //���� ������
			pkt = 0 ;
			pkt = new Packet();
			pkt->AddHeader(ph);
			pkt->AddHeader(datah);
		}
		m_propDelay[ch.GetSrc()] = Simulator::Now() - datah.GetTimeStamp() - Seconds (m_DATA * 8.0 / m_currentRate); // ����ð� - �����ð� = (�����ð�+��Ŷ���۽ð�) - ��Ŷ���۽ð�
		break;

	case TYPE_RTS:
		pkt->RemoveHeader(rtsh);
		if ( ch.GetDest() == m_address)
		{
			pkt->AddHeader(rtsh);
		}
		m_propDelay[ch.GetSrc()] = Simulator::Now() - rtsh.GetTimeStamp() - Seconds (m_RTS * 8.0 / m_currentRate); // ����ð� - �����ð� = (�����ð�+��Ŷ���۽ð�) - ��Ŷ���۽ð�
		break;

	case TYPE_CTS:
		pkt->RemoveHeader(ctsh);
		pkt->RemoveHeader(ph);
		pkt = 0;

		if ( ch.GetDest() == m_address)
		{
			pkt = m_pktTx.front().first; //���� ������ ��Ŷ �߰�
			m_pktTx.front().first = 0 ;
			m_pktTx.pop_front();
			m_pktTxProt.pop_front();


			pkt->AddHeader(ph);
			pkt->AddHeader(ctsh);
		}
		m_propDelay[ch.GetSrc()] = Simulator::Now() - ctsh.GetTimeStamp() - Seconds (m_CTS * 8.0 / m_currentRate); // ����ð� - �����ð� = (�����ð�+��Ŷ���۽ð�) - ��Ŷ���۽ð�
		break;

	case TYPE_ACK:
		pkt->RemoveHeader(ackh);
		if ( ch.GetDest() == m_address)
		{
			pkt->AddHeader(ackh);
		}
		m_propDelay[ch.GetSrc()] = Simulator::Now() - ackh.GetTimeStamp() - Seconds (m_ACK * 8.0 / m_currentRate); // ����ð� - �����ð� = (�����ð�+��Ŷ���۽ð�) - ��Ŷ���۽ð�
		break;

	default : 
		if ( DEBUG_PRINT > 1) {
			cout << "Unknown packet type " << ch.GetType () << " received at node " << (uint32_t)m_address.GetAsInt()<< "\n";
		}
		NS_FATAL_ERROR ("Unknown packet type " << ch.GetType () << " received at node " << (uint32_t)m_address.GetAsInt());
	}

	

	if ( DEBUG_PRINT > 2)
	{
		cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " src " << (uint32_t)ch.GetSrc().GetAsInt() << "-> dest " << (uint32_t)m_address.GetAsInt() << " " << m_propDelay[ch.GetSrc()].GetSeconds() << " propDelay�ð� \n";
		cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " src " << (uint32_t)ch.GetSrc().GetAsInt() << "-> dest " << (uint32_t)m_address.GetAsInt() << " " << Seconds (bytes * 8.0 / m_currentRate).GetSeconds() << " ��Ŷ���۽ð�\n ";
		cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " src " << (uint32_t)ch.GetSrc().GetAsInt() << "-> dest " << (uint32_t)m_address.GetAsInt() << " " << m_propDelay[ch.GetSrc()].GetSeconds() + Seconds (bytes * 8.0 / m_currentRate).GetSeconds() << " ����+��Ŷ���۽ð�\n ";
	}

	//�� ��Ŷ�ΰ� �ƴѰ��� �����Ѵ�.
	if ( ch.GetDest() == m_address )
	{
		/*�����ð� ���*/
		pkt->AddHeader(ch);
		m_transPktTx = pkt ;

		/*
		--����--
		0. �۽����� timerEvent �����ؼ� ������ ������ �Ϸ��ߴٰ� �˸�
		SIFS event�� �߻���Ų��.
		--SIFS--
		1. ���ο� common����� ����� ������ ������ �ּҸ� ���� �Ѵ�.
		*/
		
		/* timer �̺�Ʈ ���� */
		if ( m_sendTime != Seconds(0) )
		{
			if ( DEBUG_PRINT > 1)
				cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " m_sendEvent ���� \n";
			Simulator::Cancel(m_sendEvent);
			m_sendTime = Seconds(0);
		}
		m_prevState = m_state;
		m_state = RUNNING;

		switch (ch.GetType ())
		{
			/* 
			--SIFS--
			0. common����� m_type�� rts->cts, cts->data, data->ack�� �����Ѵ�.
			1. data/rts/cts/ack header ����
			2.  ��Ŷ ������ ���۽ð��� ��� ���ϴ���?
			��ó�� duration ���� packet->duration = (RTS / BasicRate) + (CTS / BasicRate) + payload_tx_time(packet->soucAddress, packet->destAddress1) + (ACK / BasicRate) + SIFS * 3;
			rts reply_packet->duration = act_node->receive_packet.duration - (RTS / act_node->receive_packet.dataRate + SIFS); // - RTS���ۼӵ� + SIFS�ð�
			cts reply_packet->duration = act_node->receive_packet.duration - (CTS / act_node->receive_packet.dataRate + SIFS);
			data reply_packet->duration = act_node->receive_packet.duration - (payload_tx_time(reply_packet->soucAddress, reply_packet->destAddress1) + SIFS);
			3. data/rts/cts/ack�� ����� ������ duration �ʵ� ���� ������ �� ����� �����.
			--�۽�--
			2. timer Event(����ð�+ SIFS�ð� ���� ���� ��������Ŷ�� ����(rts/cts/data/ack)+ ���� �����ð� + ��밡 ���� ��������Ŷ�� ����+ ���������ð� )�� �����Ѵ�. //timerevent�� �߻��ϸ� ���������� ������
			//timerEvent�� �ð��� ����ϴ� �����?!
			uint32_t ctsBytes = ch.GetSerializedSize () + pkt->GetSize ();
			double m_currentRate = m_phy->GetMode (m_currentRate).GetDataRateBps ();
			m_learnedProp = Simulator::Now () - ctsg.GetTxTimeStamp () - Seconds (ctsBytes * 8.0 / m_currentRate);
			4. sendPacket(��Ŷ) ����
			//Ÿ�̸Ӵ� rts/cts/data/ack�� sendPacket�տ��� �ߵ��ؾ���
			*/

		case TYPE_DATA:
			{
			/*������ ��Ŷ������ �ٸ����κ��� ��Ŷ�� �޾� ���� NAV������ ���*/
			if ( m_navTime != Seconds(0) && m_waitFlag == false)
			{
				if ( DEBUG_PRINT >1)
					cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " ���� NAV�� DATA��Ŷ �޾����� SIFS �̺�Ʈ �ߵ� ���� \n";
				return;
			}
			//������������ ����
			m_forwardUpCb (dataPkt, ch.GetSrc ());

			//��Ŷ�� ���� ���Ӱ� ���� ���������� ���ſ�

			
			m_transPktTx = 0 ;
			m_transPktTx = new Packet();
			m_transPktTx->AddHeader(ph);
			m_transPktTx->AddHeader(datah);
			m_transPktTx->AddHeader(ch);

			if ( datah.GetSelectedNode() == NOT_FOUND )
			{
				// �׳� ��Ҵ�� ����
				if ( m_navTime != Seconds(0) )
				{
					Simulator::Cancel(m_navEndEvent);
					m_navTime = Seconds(0);
					if ( DEBUG_PRINT > 1) {
						cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": �������� ������ ��ü ���ſϷ� " << "NAV ���� \n";
					}

					m_transPktTx = m_savePktTx;
				}
				if ( DEBUG_PRINT > 1) {
					cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " navTime : " << m_navTime.GetSeconds() <<"\n";
				}

				
			}
			else
			{
				//AP�� ��� �ٸ� �����Ͱ� �������� �ü� �ִ��� Ȯ�� �� NAV�� ������
				//�����͸� ���������� �ް� �ٽ� SIFS�� ���۽�Ű���
				if ( m_selectedNodes.size() == 0 )
				{
					m_savePktTx = m_transPktTx;
				}
				m_selectedNodes.push_back(datah.GetSelectedNode());

				Time tempNavTime = datah.GetArrivalTime(); //�ӽ÷� ����

				if ( m_navTime != Seconds(0)  ) // ������ NAV < ������ NAV�� ��� ū �ð����� ���
				{
					Time nextNavTime = tempNavTime + Simulator::Now();
					if ( m_navTime < nextNavTime )
					{
						Simulator::Cancel(m_navEndEvent);
						m_navTime = tempNavTime ;
						m_navEndEvent = Simulator::Schedule(m_navTime, &UanMacCw1::navTimer, this);
						m_navTime = Simulator::Now () + m_navTime;

						if ( DEBUG_PRINT >1)
							cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ������ NAV ���� �� NAV�̺�Ʈ ���� " << m_navTime.GetSeconds() << " : NAV����ð�\n";
					}
				}
				else
				{
					m_navEndEvent = Simulator::Schedule (tempNavTime, &UanMacCw1::navTimer, this); // 

					if ( DEBUG_PRINT > 1)
						cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": NAV�̺�Ʈ ���� " << (Simulator::Now ()+tempNavTime).GetSeconds() << " : NAV����ð�\n";
			
					m_navTime = tempNavTime + Simulator::Now();
				}
				if ( DEBUG_PRINT > 1) {
					cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " tempNavTime :" << m_navTime.GetSeconds() <<"\n";
				}
				m_prevState = m_state;
				m_state = NAV;
				m_waitFlag = true ;
				return ;
			}
			// �ʰԿ� ��尡 dest�� �����Ǵ� ���̷����� ��Ȳ �߻�

			m_waitFlag = false ;
			/*SIFS�� ���� �������� ��Ŷ�� ��Ʈ��ȣ*/
			m_sifsEvent =  Simulator::Schedule (Seconds(m_sifs), &UanMacCw1::sifsTimer, this); // ACK ���� SIFS
			if ( DEBUG_PRINT > 1)
				cout << "Time " << (Simulator::Now()).GetSeconds() << " Addr "  << (uint32_t)m_address.GetAsInt() << " SIFSEvent �̺�Ʈ ���� " << " Time "<< (Simulator::Now()+Seconds(m_sifs)).GetSeconds() << "�� SIFSEvent �̺�Ʈ �ߵ�\n";

			m_sifsTime = Simulator::Now() + Seconds(m_sifs);
			}
				break;
		case TYPE_RTS:
			{
			/*������ ��Ŷ������ �ٸ����κ��� ��Ŷ�� �޾� ���� NAV������ ���*/
			if ( m_navTime != Seconds(0))
			{
				if ( DEBUG_PRINT > 1)
					cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " ���� NAV�� RTS��Ŷ �޾����� SIFS �̺�Ʈ �ߵ� ���� \n";
				return;
			}
			m_sifsEvent =  Simulator::Schedule (Seconds(m_sifs), &UanMacCw1::sifsTimer, this); // CTS ���� SIFS
			if ( DEBUG_PRINT > 1)
				cout << "Time " << (Simulator::Now()).GetSeconds() << " Addr "  << (uint32_t)m_address.GetAsInt() << " SIFSEvent �̺�Ʈ ���� " << " Time " << (Simulator::Now()+Seconds(m_sifs)).GetSeconds() << "�� SIFSEvent �̺�Ʈ �ߵ�\n";
			m_sifsTime = Simulator::Now() + Seconds(m_sifs);
			}
			break;
		case TYPE_CTS:
			{
			/*������ ��Ŷ������ �ٸ����κ��� ��Ŷ�� �޾� ���� NAV������ ���*/
			if ( m_navTime != Seconds(0))
			{
				if ( DEBUG_PRINT > 1)
					cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " ���� NAV�� CTS��Ŷ �޾����� SIFS �̺�Ʈ �ߵ� ���� \n";
				return;
			}
			m_sifsEvent =  Simulator::Schedule (Seconds(m_sifs), &UanMacCw1::sifsTimer, this); // DATA ���� SIFS
			if ( DEBUG_PRINT > 1)
				cout << "Time " << (Simulator::Now()).GetSeconds() << " Addr "  << (uint32_t)m_address.GetAsInt() << " SIFSEvent �̺�Ʈ ���� " << " Time "<< (Simulator::Now()+Seconds(m_sifs)).GetSeconds() << "�� SIFSEvent �̺�Ʈ �ߵ�\n";
			m_sifsTime = Simulator::Now() + Seconds(m_sifs);
			
			}
			break;
		case TYPE_ACK:
			processingSuccess();
			
			break;
		default:
			if ( DEBUG_PRINT > 1)
				cout << "Unknown packet type " << ch.GetType () << " received at node " << (uint32_t)m_address.GetAsInt()<<"\n";
			NS_FATAL_ERROR ("Unknown packet type " << ch.GetType () << " received at node " << (uint32_t)m_address.GetAsInt());
		}
	}
	else //���� �ƴϴϱ� NAV �̺�Ʈ��
	{
		/*
		1. �ڽ��� ���°� NAV�� ���
		1.1 ���� NAV�� duration���� ���Ͽ� ���� nav�ð� <  ���� nav �ð��� ���
		���� nav�ð� = ���� nav�ð��� �־ ������

		2. �ڽ��� ���°� NAV�� �ƴѰ��
		2.1 RTS/CTS/DATA/ACK�� ����� �и��Ѵ�.
		2.2 NAVEvent(����ð� + duration) �� �����Ѵ�.
		*/

		if ( ch.GetType() == TYPE_ACK && (ackh.GetFirstNode() == (uint32_t)m_address.GetAsInt() || ackh.GetSecondNode() == (uint32_t)m_address.GetAsInt()))
		{
			/* �������� ������ ���۹�� ���� */
			if ( TRANSMODE == 3)
			{
				if ( m_backoff != Seconds(0) || m_backoffTime != Seconds(0))
				{
					if (m_backoffTime !=Seconds(0))
					{
						Simulator::Cancel(m_backOffEvent);
						m_backoffTime = Seconds(0);
					}
					m_backoff = Seconds(0);
					m_reset_cw += m_reset_cw;
				}
			}
			processingSuccess();
			
			return;
		}
		if ( m_sendTime != Seconds(0) )
		{
			if ( DEBUG_PRINT > 1) {
				cout << "Time " << Simulator::Now().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " sendTimer �۵����̹Ƿ� NAV�̺�Ʈ ���� ���� ���� ��Ŷ����\n";
			}
			m_prevState = m_state ;
			m_state = IDLE;
			return ;
		}
		
		//ex) RTS + CTS + DATA + ACK -> CTS+ DATA + ACK 

		/* 2017-05-31 ��������
		NAV�� �����ð��� maxdistance/speed �ð��� ���������
		*/

		Time propDelay = Seconds(0);
		Time tempNavTime = Seconds(0) ;

		switch(ch.GetType())
		{
		case TYPE_RTS:
			tempNavTime = rtsh.GetDuration() - Seconds(m_RTS * 8.0 / m_currentRate); // - Seconds(m_maxPropDelay)  ; // 
			//propDelay = m_propDelay[ch.GetDest()]+m_propDelay[ch.GetSrc()] + m_propDelay[ch.GetDest()];// CTS����, DATA����, ACK����
			//propDelay = Seconds(m_maxPropDelay)  + Seconds(m_maxPropDelay) + Seconds(m_maxPropDelay) ;
			break;
		case TYPE_CTS:
			tempNavTime = ctsh.GetDuration() - Seconds(m_CTS * 8.0 / m_currentRate) - Seconds(m_maxPropDelay);
			//propDelay = m_propDelay[ch.GetSrc()]+m_propDelay[ch.GetDest()]; // DATA����, ACK����
			//propDelay = Seconds(m_maxPropDelay)  + Seconds(m_maxPropDelay)  ;
			break;
		case TYPE_DATA:
			/* �������� ������ ���۹�� ���� */
			if (datah.GetSelectedNode() == (uint32_t)m_address.GetAsInt())
			{
				setCtsPacket(ch.GetSrc().GetAsInt());
				// �����ð� �� SIFS����
				// �����ð� - datah�޴µ� �ɸ� �����ð�
				Time arrival = datah.GetArrivalTime() - (Simulator::Now() - datah.GetTimeStamp()); //arrival �ð��� �� ��Ȯ�ϰ� ����ؾ���
				// ���õ� ����� �Ÿ����� AP���� �Ÿ����� �� ��� �߻���
				if ( arrival < 0 ) {
					arrival = Seconds(0) ;
				}
				if ( DEBUG_PRINT > 1 ) {
					cout << "datah.GetArrivalTime() : " << datah.GetArrivalTime() << " Simulator::Now() : " << Simulator::Now() << " datah.GetTimeStamp() : " << datah.GetTimeStamp()<< "\n";  
					cout << "(Simulator::Now() - datah.GetTimeStamp()) : " << (Simulator::Now() - datah.GetTimeStamp()) << '\n';
				}

				/*SIFS�� ���� �������� ��Ŷ�� ��Ʈ��ȣ*/
				m_sifsEvent =  Simulator::Schedule (arrival, &UanMacCw1::sifsTimer, this); // ACK ���� SIFS
				if ( DEBUG_PRINT > 1)
					cout << "Time " << (Simulator::Now()).GetSeconds() << " Addr "  << (uint32_t)m_address.GetAsInt() << ": ������ NAV ���� �� �������� ������ ������ ���� SIFS ���� " << (Simulator::Now()+arrival).GetSeconds() << "�� SIFSEvent �̺�Ʈ �ߵ�\n";
				m_sifsTime = Simulator::Now() + arrival;
				return ;
			}

			tempNavTime = datah.GetDuration() - Seconds (m_DATA * 8.0 / m_currentRate) - Seconds(m_maxPropDelay);
			//propDelay = m_propDelay[ch.GetDest()]; // ACK����
			//propDelay = Seconds(m_maxPropDelay)  ;
			break;
		case TYPE_ACK:
			//����ġ �߰�
			addExcetionCount((uint32_t)m_distance[ch.GetSrc().GetAsInt()].second);
			tempNavTime = ackh.GetDuration() - Seconds (m_ACK * 8.0 / m_currentRate) - Seconds(m_maxPropDelay);
			break;
		}

		
		tempNavTime += propDelay; // duration + ����

		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " ���� duration + ���� -> " << (Simulator::Now()+tempNavTime).GetSeconds() << "\n" ;

		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " ���� duration + ���� -> " << m_navTime.GetSeconds() << "\n" ;


		if ( m_navTime != Seconds(0)  ) // ������ NAV < ������ NAV�� ��� ū �ð����� ���
		{
			Time nextNavTime = tempNavTime + Simulator::Now();
			if ( m_navTime < nextNavTime )
			{
				Simulator::Cancel(m_navEndEvent);
				m_navTime = tempNavTime ;
				m_navEndEvent = Simulator::Schedule(m_navTime, &UanMacCw1::navTimer, this);
				m_navTime = Simulator::Now () + m_navTime;

				if ( DEBUG_PRINT >1)
					cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ������ NAV ���� �� NAV�̺�Ʈ ���� " << m_navTime.GetSeconds() << " : NAV����ð�\n";
			}

			m_prevState = m_state;
			m_state = NAV;
		}
		else
		{
			m_navEndEvent = Simulator::Schedule (tempNavTime, &UanMacCw1::navTimer, this); // 

			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": NAV�̺�Ʈ ���� " << (Simulator::Now ()+tempNavTime).GetSeconds() << " : NAV����ð�\n";
			
			m_navTime = tempNavTime + Simulator::Now();
			m_prevState = m_state;
			m_state = NAV;
		}
	}
}
void
UanMacCw1::PhyRxPacketError (Ptr<Packet> packet, double sinr)
{
	if ( DEBUG_PRINT > 2 )
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << " PhyRxRacketError\n";
}

void UanMacCw1::navTimer()
{
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": NAV�̺�Ʈ �ߵ�\n";

	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� TX �����̹Ƿ� NAV �ߵ� ����\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� TX �����̹Ƿ� NAV �ߵ� ����");
	}
	
	if ( m_sendTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� sendTimer �����̹Ƿ� NAV �̺�Ʈ ����\n";
		m_navTime = Seconds(0);
		return ;
	}

	if ( m_state == RX)
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� RX �����̹Ƿ� NAV �̺�Ʈ ����\n";
		m_navTime = Seconds(0);
		return ;
	}

	m_navTime = Seconds(0);
	m_prevState = m_state ;
	m_state = IDLE;

	if ( m_pktTx.size() > 0 )
	{
		if ( isIDLE() )
		{
			m_prevState = m_state;
			m_state = RUNNING;

			//difsevent ����
			m_difsEvent = Simulator::Schedule (Seconds(m_difs), &UanMacCw1::difsTimer, this); // 2*SlotTime(m_sifs)+ SIFS(m_sifs)
			m_difsTime = Simulator::Now()+ Seconds(m_difs);
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": DIFS �̺�Ʈ ����" << " Time " <<m_difsTime.GetSeconds() << "�� DIFS �̺�Ʈ �ߵ�\n";
			//NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": DIFS �̺�Ʈ ����" << " Time " <<(Simulator::Now ()+Seconds(2*m_slotTime.GetSeconds())+ Seconds(m_sifs)).GetSeconds() << "�� DIFS �̺�Ʈ �ߵ�");
		}
	}

}
void UanMacCw1::difsTimer()
{
	/*
		  1. backoff event(����ð� + cw(����) *����Ÿ�� )�� �߻��Ѵ�.
		  2. backoff event�� �ߵ��ϸ� sendRtsEvent�� �����Ѵ�.
	*/
	if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " difs �̺�Ʈ �ߵ�\n";

	m_difsTime = Seconds(0);
	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� TX �����̹Ƿ� difs �ߵ� ����\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� TX �����̹Ƿ� difs �ߵ� ����");
	}
	if ( m_state == RX )
	{
		if ( DEBUG_PRINT >1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� RX �����̹Ƿ� difs �̺�Ʈ ����\n";
		Simulator::Cancel(m_difsEvent);
		return ;
	}

	if ( m_navTime != Seconds(0) )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� NAV �����̹Ƿ� difs �̺�Ʈ ����\n";
		Simulator::Cancel(m_difsEvent);
		return ;
	}



	if ( m_sendTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� sendTimer �����̹Ƿ� difs �̺�Ʈ ����\n";
		return ;
	}

	if (m_startPacket.empty() )
	{
		m_startPacket.push_back(Simulator::Now());
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ��Ŷ ����� ���� -> " << Simulator::Now().GetSeconds() << "\n";
		
	}
	

	if ( m_pktTx.size() > 0 )
	{

		
		m_prevState = m_state;
		m_state = BACKOFF;

		if (m_backoff != Seconds(0))
		{
			m_backoffTime = m_backoff;
			if ( DEBUG_PRINT >1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << ": ���� BackoffTime -> " << m_backoff.GetSeconds() << " BACKOFF�̺�Ʈ ���� " << " Time " << (Simulator::Now() + m_backoffTime).GetSeconds() << "�� backoff �ߵ�\n";
		}
		else
		{// bc ���� 0�̶�� ������(0~CW) * slot_time �ð����� �̺�Ʈ �ð� ���� 

			uint32_t cw = (uint32_t) m_rv->GetValue (0,m_cw);
			m_backoffTime = Seconds ((double)(cw) * m_slotTime.GetSeconds ());
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " cw-> " << cw <<" : BACKOFF�̺�Ʈ ���� " << " Time " << (Simulator::Now() + m_backoffTime).GetSeconds() << "�� backoff �ߵ�\n";
		}

		//backoffevent ����
		m_backOffEvent = Simulator::Schedule (m_backoffTime, &UanMacCw1::backOffTimer, this); // 2*SlotTime(m_sifs)+ SIFS(m_sifs)
		m_backoffTime += Simulator::Now(); // ���߿� ���� ����� �ð��� ����ϱ� ���ؼ�

	}

}
void UanMacCw1::sendTimer()
{
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt()<< ": sendTimer�̺�Ʈ �ߵ�\n";

	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� TX �����̹Ƿ� sendTimer �ߵ� ����\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� TX �����̹Ƿ� sendTimer �ߵ� ����\n");
	}
	
	m_sendTime = Seconds(0);
	if ( m_pktTx.size() > 0 )
	{
		if ( isIDLE() )
		{
			m_prevState= m_state;
			m_state= RUNNING;
			m_difsEvent = Simulator::Schedule ( Seconds(m_difs), &UanMacCw1::difsTimer, this); // 2*SlotTime(m_sifs)+ SIFS(m_sifs)	
			m_difsTime = Seconds(m_difs) + Simulator::Now();
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << ": DIFS�̺�Ʈ ���� " << m_difsTime.GetSeconds() << "�� DIFS �̺�Ʈ �ߵ�\n";
		}
		
	}

	//RTS�� ��츸 ����ó��
	if ( m_sendType == RTS)
	{
		m_failCount++;
		m_cw = m_cw * 2;
		m_retryCnt ++;
		m_sendType = UNKNOWN;
		if ( DEBUG_PRINT > 0)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���۽���!! failCount -> " << m_failCount  << " m_retryCnt -> " << m_retryCnt << " cw-> " << m_cw << "\n";

		//3�� ���� �� �ʱ�ȭ
		if ( m_retryCnt >= m_RetryLimit )
		{
			m_cw = 10;
			m_retryCnt = 0 ;
			m_backoff = Seconds(0);
			m_backoffTime = Seconds(0);
			m_pktTx.pop_front();
			m_pktTxProt.pop_front();
			m_startPacket.clear();

			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " �ش� ��Ŷ 3�� ���� ���۽���!! ��Ŷ ������ cw�ʱ�ȭ\n";
		}
	}
}
void UanMacCw1::backOffTimer()
{
	/*
	1. backoffTimer : backoff�� �Ϸ��Ͽ� ������ �����ϴ� �ܰ�
	2. RTS�� ������ CTS�� �޴µ����� �ɸ��� �ð��� �����ؾ���
	3. sendPacket ����
	*/
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " BackOffEvent �ߵ�\n";
	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� TX �����̹Ƿ� backoffTimer �ߵ� ����\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� TX �����̹Ƿ� backoffTimer �ߵ� ����");
	}
	if ( m_state == RX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� RX �����̹Ƿ� backoffTimer �ߵ� ����\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� RX �����̹Ƿ� backoffTimer �ߵ� ����");
	}
	if ( m_sifsTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� SIFS �����̹Ƿ� backoffTimer �ߵ� ����\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� SIFS �����̹Ƿ� backoffTimer �ߵ� ����");
	}

	m_backoff = Seconds(0) ;
	m_sendType = RTS;

	// 1.rts����� ����� ����ð��� ��� -> ���߿� �����ð� ������
	// 2. common����� ����� src , dest�� �����Ѵ�.

	//ť���� ��Ŷ�� ������
	std::pair <Ptr<Packet>, UanAddress > m_p = m_pktTx.front();
	Ptr<Packet> transPkt = new Packet();

	//common ����� ����� ����� ������ ��Ŷ Ÿ�� ����
	UanHeaderCommon header;
	header.SetDest(m_p.second);
	header.SetSrc (m_address);
	header.SetType (TYPE_RTS);
	
	//RTS��� �����Ͽ� ����ð� �߰�
	UanHeaderRcRts1 rtsHeader ; 
	rtsHeader.SetTimeStamp(Simulator::Now());

	//������ ���� �ð��� ���� ������ ��� �߰�
	UanHeaderRcPhy ph;

	// data(200) + phy(17) + data(27) + common(3) -> 247
		// phy(17) + rts(17) + common(3) -> 37
		// phy(17) + cts(11) + common(3) -> 31
		// phy(17) + ack(14) + common(3) -> 34

	//Time duration =  Seconds( ((m_RTS + m_CTS + m_DATA + m_ACK  )*8.0) / m_currentRate ) + Seconds(m_sifs*3) ; // (RTS + CTS +  DATA + ACK )/bps + SIFS * 3
	/* 2017-06-22 duration�� m_propDealy�߰�*/	
	Time duration =  Seconds( ((m_RTS + m_CTS + m_DATA + m_ACK  )*8.0) / m_currentRate ) + Seconds(m_sifs*3) + Seconds(m_maxPropDelay*4) ; // (RTS + CTS +  DATA + ACK )/bps + SIFS * 3
	
	//��Ŷ�� ������ �ٸ� ���� ������ ����� ���� ���� �־�� �ϴ� �ð��� ����ϱ� ���� ����
	m_sleepTime = duration + Simulator::Now();

	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " " << m_sleepTime.GetSeconds() <<"�� ���ۿϷ�ð� \n";

	//duration ����
	rtsHeader.SetDuration(duration); // (typeheader �����½ð� + commonheader�����½ð� + packect �����½ð�)*8*4 + SIFS*3 //8����Ʈ
	rtsHeader.SetProtocolNumber(m_pktTxProt.front());

	//��Ŷ�� ��� �߰�
	transPkt->AddHeader (ph);
	transPkt->AddHeader (rtsHeader);
	transPkt->AddHeader (header);

	//sendtimer ����
	uint32_t Bytes = transPkt->GetSize ();
	
	Time currentPropDelay ;
	//������ ���Ǳ� ������ max �����ð����� ����ϴٰ� ���ΰ��� ����ϸ鼭 ������ �˾ư��� �˾Ƴ� �������� ��ü
	if ( m_propDelay[(uint32_t)m_p.second.GetAsInt()] != Seconds(0) )
	{
		//NS_LOG_DEBUG (m_propDelay[(uint32_t)m_p.second.GetAsInt()].GetSeconds());
		m_sendTime = (m_propDelay[(uint32_t)m_p.second.GetAsInt()]) + (m_propDelay[(uint32_t)m_p.second.GetAsInt()]) + Seconds(m_sifs) + Seconds(m_RTS*8.0/m_currentRate)+ Seconds(m_CTS*8.0/m_currentRate) + Seconds(m_mifs); // �����ð�*2 + SIFS + ��Ŷ���۽ð�*2
		currentPropDelay = m_propDelay[(uint32_t)m_p.second.GetAsInt()];
	}
	else
	{
		m_sendTime = Seconds(m_maxPropDelay*2) + Seconds(m_sifs) + Seconds(m_RTS*8.0/m_currentRate)+ Seconds(m_CTS*8.0/m_currentRate) + Seconds(m_mifs); // �����ð�*2 + SIFS + ��Ŷ���۽ð�*2
		currentPropDelay = Seconds(m_maxPropDelay);
	}

	
	m_sendEvent = Simulator::Schedule (m_sendTime, &UanMacCw1::sendTimer, this); // RTS����������+CTS�޴�����+ ������ ��Ŷ���ۼӵ� + �޴� ��Ŷ���ۼӵ� + SIFS
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)m_p.second.GetAsInt() << ": sendTimer�̺�Ʈ ���� " << (Simulator::Now() +m_sendTime).GetSeconds() << "�� sendTimer�̺�Ʈ �ߵ�\n";
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)m_p.second.GetAsInt() << ": RTS ��Ŷ ���� " << (Simulator::Now() +Seconds(m_RTS*8.0/m_currentRate) +currentPropDelay).GetSeconds() << "�� RTS ��Ŷ ����\n";

	m_sendTime = Simulator::Now() + m_sendTime;

	//if ( DEBUG_PRINT > 1)
		//	cout <<  "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " packet size->" << transPkt->GetSize() << "\n";

	//��Ŷ����
	m_phy->SendPacket (transPkt,m_pktTxProt.front());
	//delete����
	transPkt = 0 ;
	m_backoffTime = Seconds(0);
}
void UanMacCw1::sifsTimer()
{
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " SIFSTimer �̺�Ʈ �ߵ�\n"  ;

	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� TX �����̹Ƿ� sifs �ߵ� ����\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� TX �����̹Ƿ� sifs �ߵ� ����");
	}
	if ( m_state == RX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� RX �����̹Ƿ� sifs �ߵ� ����\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� RX �����̹Ƿ� sifs �ߵ� ����");
	}
	if ( m_state == NAV )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� NAV �����̹Ƿ� sifs �ߵ� ����\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ���� NAV �����̹Ƿ� sifs �ߵ� ����");
	}

	//common��� �κ��� ������
	UanHeaderCommon ch;
	m_transPktTx->RemoveHeader (ch);

	UanHeaderRcData1 datah;
	UanHeaderRcRts1 rtsh;
	UanHeaderRcCts1 ctsh;
	UanHeaderRcAck1 ackh;
	UanHeaderRcPhy ph;
	
	
	UanHeaderCommon transCh;
	UanHeaderRcCts1 transCtsh;
	UanHeaderRcData1 transDatah;
	UanHeaderRcAck1 transAckh;
	uint16_t protNum = 0 ;

	//1. RTS/CTS/DATA/ACK�� ����� �и��Ѵ�.
	transCh.SetDest(ch.GetSrc());
	transCh.SetSrc(ch.GetDest());

	switch (ch.GetType ())
	{
		case TYPE_RTS:
			{
			m_transPktTx->RemoveHeader(rtsh);
			m_sendType = CTS;
			transCh.SetType(TYPE_CTS);

			transCtsh.SetTimeStamp(Simulator::Now());
			transCtsh.SetDuration(rtsh.GetDuration() - Seconds (m_RTS * 8.0 / m_currentRate) - Seconds(m_sifs) -Seconds(m_maxPropDelay));  // ���� SIFS���� ���� ��Ȳ�̱� ������ RTS�� SIFS�ð��� �������
			transCtsh.SetProtocolNumber(rtsh.GetProtocolNumber());
			protNum = rtsh.GetProtocolNumber();
			
			m_transPktTx->AddHeader(transCtsh);
			m_sendTime = m_propDelay[ch.GetSrc()] + m_propDelay[ch.GetSrc()] + Seconds( ((m_CTS*8.0)/m_currentRate)) + Seconds( ((m_DATA*8.0)/m_currentRate)) + Seconds(m_sifs) + Seconds(m_mifs); // �����ð�*2 + SIFS + ��Ŷ���۽ð�*2 + mifs

			m_sendEvent = Simulator::Schedule (m_sendTime, &UanMacCw1::sendTimer, this); // RTS����������+CTS�޴�����+ ������ ��Ŷ���ۼӵ� + �޴� ��Ŷ���ۼӵ� + SIFS
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << ": sendTimer�̺�Ʈ ���� " << (Simulator::Now() + m_sendTime).GetSeconds() << "�� sendTimer�̺�Ʈ �ߵ�\n";
			m_sendTime = Simulator::Now() + m_sendTime;

			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << ": CTS ��Ŷ ���� "<< (Simulator::Now()+m_propDelay[ch.GetSrc()] + Seconds( ((m_CTS*8.0)/m_currentRate))).GetSeconds() << "�� CTS ��Ŷ ����\n";
			}
			break;
		case TYPE_CTS:
			{

			m_transPktTx->RemoveHeader(ctsh);
			m_sendType = DATA;
			transCh.SetType(TYPE_DATA);

			/* �������� ������ ���۾˰��� �߰� �κ� */
			uint32_t selectedNode = NOT_FOUND;

			if (m_startPacket.empty() )
			{
				m_startPacket.push_back(Simulator::Now());
				if ( DEBUG_PRINT > 1)
					cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " ��Ŷ ����� ���� -> " << Simulator::Now().GetSeconds() << "\n";
			}

			if (TRANSMODE == 1)
			{
				selectedNode = getRandomNode();
			}
			else if (TRANSMODE == 2)
			{
				selectedNode = getNearestNode();
			}
			else if (TRANSMODE == 3)
			{
				selectedNode = getFairnessNode();
			}
			
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now().GetSeconds () << " " << (uint32_t)m_address.GetAsInt() << " selecetdNode-> "  << selectedNode << "\n";

			//���� ���� AP������ �����ð� + ������ ���۽ð�
			//Ack�� AP�� �����͸� ������ ���� �� �ֱ⿡ �ð����� ���ܽ��� ����
			Time arrivalTime = m_propDelay[ch.GetSrc()] + Seconds(m_DATA*8.0/m_currentRate) ;

			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << " arrivalTime->  " << (Simulator::Now() +arrivalTime).GetSeconds() << "�� ������ ��Ŷ ����\n";

			transDatah.SetArrivalTime(arrivalTime);
			transDatah.SetSelectedNode(selectedNode);
			transDatah.SetTimeStamp(Simulator::Now());
			transDatah.SetDuration(ctsh.GetDuration() - Seconds (m_CTS * 8.0 / m_currentRate) - Seconds(m_sifs) - Seconds(m_maxPropDelay));  // data ��Ŷ�� �����Ⱥ��±⶧���� byte���� 200��ŭ�� ������ ��Ŷ ���� �������
			transDatah.SetProtocolNumber(ctsh.GetProtocolNumber());
			protNum = ctsh.GetProtocolNumber();
			m_transPktTx->AddHeader(transDatah);

			m_sendTime = m_propDelay[ch.GetSrc()] + m_propDelay[ch.GetSrc()] + Seconds(m_DATA*8.0/m_currentRate)+ Seconds(m_ACK*8.0/m_currentRate) + Seconds(m_sifs)+ Seconds(m_mifs); // �����ð�*2 + SIFS + ��Ŷ���۽ð�*2 + mifs

			for ( int i = 1 ; i < m_distance[(uint32_t)m_address.GetAsInt()].second; ++i)
			{
				m_sendTime += m_sendTime; //sendTime�ð� ����� �� ��Ȯ�ϰ� �ؾ���
			}
			m_sendEvent = Simulator::Schedule (m_sendTime, &UanMacCw1::sendTimer, this); // RTS����������+CTS�޴�����+ ������ ��Ŷ���ۼӵ� + �޴� ��Ŷ���ۼӵ� + SIFS
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << ": sendTimer�̺�Ʈ ���� " << (Simulator::Now() +m_sendTime).GetSeconds() << "�� sendTimer�̺�Ʈ �ߵ�\n";
			m_sendTime = Simulator::Now() + m_sendTime;
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << ": DATA ��Ŷ ���� "<< (Simulator::Now()+  m_propDelay[ch.GetSrc()] + Seconds(m_DATA*8.0/m_currentRate)).GetSeconds() << "�� DATA ��Ŷ ����\n";
			}
			break;
		case TYPE_DATA:
			{
			m_transPktTx->RemoveHeader(datah);
			m_sendType = ACK;
			transCh.SetType(TYPE_ACK);
			transAckh.SetTimeStamp(Simulator::Now());
			transAckh.SetDuration(datah.GetDuration() - Seconds (m_DATA * 8.0 / m_currentRate) - Seconds(m_sifs) -Seconds(m_maxPropDelay)); 
			transAckh.SetProtocolNumber(datah.GetProtocolNumber());
			transAckh.SetFirstNode(NOT_FOUND);
			transAckh.SetSecondNode(NOT_FOUND);

			if ( m_selectedNodes.size() == 1 )
			{
				transAckh.SetFirstNode(m_selectedNodes[0]);
			}
			else if ( m_selectedNodes.size() == 2 )
			{
				transAckh.SetFirstNode(m_selectedNodes[0]);
				transAckh.SetSecondNode(m_selectedNodes[1]);
			}

			protNum = datah.GetProtocolNumber();
			m_transPktTx->AddHeader(transAckh);
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << ": ACK ��Ŷ ���� "<< (Simulator::Now() +Seconds(m_ACK*8.0/m_currentRate) +m_propDelay[ch.GetSrc()]).GetSeconds() << "�� ACK ��Ŷ ����\n";
			}

			m_selectedNodes.clear();
			break;
	}
	

	/* ���� */
	m_transPktTx->AddHeader(transCh);
	m_phy->SendPacket(m_transPktTx,protNum);

	m_transPktTx = 0 ;
	m_sifsTime = Seconds(0);

}
bool UanMacCw1::isIDLE()
{
	if ( m_sendTime != Seconds(0))
	{
		if ( DEBUG_PRINT >1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ���� sendTimer �̺�Ʈ �� DIFS�̺�Ʈ ���� " << (m_sendTime).GetSeconds() << "�� sendTimer �̺�Ʈ �ߵ�\n";
		return false ;
	}

	if ( m_navTime != Seconds(0) || m_state == NAV)
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ���� navTimer �̺�Ʈ �� DIFS�̺�Ʈ ���� " << (m_navTime).GetSeconds() << "�� navTimer �̺�Ʈ �ߵ�\n";
		return false ;
	}

	if ( m_sifsTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ���� sifs �̺�Ʈ �� DIFS�̺�Ʈ ���� " << (m_sifsTime).GetSeconds() << "�� sifs �̺�Ʈ �ߵ�\n";
		return false ;
	}

	if ( m_difsTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ���� difs �̺�Ʈ �� DIFS�̺�Ʈ ���� " << (m_difsTime).GetSeconds() << "�� difs �̺�Ʈ �ߵ�\n";
		return false ;
	}

	if ( m_backoffTime  != Seconds(0) || m_state == BACKOFF)
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ���� backOff �̺�Ʈ �� DIFS�̺�Ʈ ���� " << (m_backoffTime).GetSeconds() << "�� backoff �̺�Ʈ �ߵ�\n";
		return false ;
	}

	if ( m_eifsTime  != Seconds(0) || m_state == EIFS)
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ���� EIFS �̺�Ʈ �� DIFS�̺�Ʈ ���� " << (m_eifsTime).GetSeconds() << "�� EIFS �̺�Ʈ �ߵ�\n";
		return false ;
	}

	if ( m_state == RX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ���� RX �̺�Ʈ �� DIFS�̺�Ʈ ����\n " ;
		return false ;
	}

	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ���� TX �̺�Ʈ �� DIFS�̺�Ʈ ����\n " ;
		return false ;
	}

	
	if ( m_state == RUNNING )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": ���� RUNNING �̺�Ʈ �� DIFS�̺�Ʈ ����\n " ;
		return false ;
	}
	
	return true ;
}
void UanMacCw1::eifsTimer()
{
	if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": EIFS �̺�Ʈ �ߵ�\n";
	
	m_eifsTime = Seconds(0);
	
	if ( m_state == RX) // eifs�߿� rx�ΰ�� rx���¸� ����
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": Rx���� �̹Ƿ� EIFS �̺�Ʈ �ߵ� ���\n";
		return ;
	}

	m_state = IDLE;

	if ( isIDLE() )
	{
		m_state = RUNNING;
		m_difsEvent = Simulator::Schedule (Seconds(m_difs), &UanMacCw1::difsTimer, this); // 2*SlotTime(m_sifs)+ SIFS(m_sifs)
		m_difsTime = Seconds(m_difs) + Simulator::Now();
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": DIFS�̺�Ʈ ���� " << m_difsTime.GetSeconds() << "�� DIFS �̺�Ʈ �ߵ�\n";
	}
}

uint32_t UanMacCw1::splitPoint(Vector2D arr[], uint32_t count)
{
	uint32_t len = m_point.length();
	uint32_t apX;
	uint32_t apY;
	uint32_t nodeNum; //��ü����� ��
	bool nodeFlag= true; // ù��° ��ü����� ���� split�� flag
	uint32_t apFlag = 0; // ap��ǥ�� �������� ���� flag
	bool xyFlag = true ; // true -> x ��ǥ, false - > y��ǥ

	std::string s="";
	int x,y;
	
	//���ø�
	for ( uint32_t i = 0 ; i < len ;++i)
	{
		if ( m_point[i] == '#') //#�� ���ø��� ����
		{
			if ( nodeFlag ) 
			{
				nodeNum =stoi(s);
				nodeFlag =false ;
				s = "";
				i++;
			}
			else if ( apFlag < 2 )
			{
				if ( apFlag == 0 )
				{
					apX = stoi(s);
					apFlag++;
					s = "";
					i++;
				}
				else
				{
					apY = stoi(s);
					apFlag++;
					s = "";
					i++;
				}
			}
			else
			{
				
				if ( xyFlag )
				{
					x = stoi(s);
					xyFlag = false;
					s = "";
					i++;
				}
				else
				{
					y = stoi(s);
					xyFlag = true;
					s = "";
					i++;
					arr[count].x = x;
					arr[count].y = y;
					count = (count+1)%m_nodeMAX;
				}
			}
		}

		s+= m_point[i];

	}
	//�������� AP��ǥ�� �־��� ������ AP�� ������ address�� ����ϱ⶧����
	arr[count].x = apX;
	arr[count].y = apY;

	return count ; // AP�� ��ġ ����
	
}
uint32_t UanMacCw1::getSection(Vector2D arr[], int currNode, int APNode)
{
	//AP���� �ٸ������� �Ÿ��� ����Ͽ� ������ ����
	double distance = CalculateDistance(arr[APNode],arr[currNode]);  //ap�� �ٸ� ����� �Ÿ� ���

	uint32_t section;
	double r = (m_boundary - arr[APNode].x)*sqrt(2) ; //1:1:sqrt(2) ��Ÿ����� ����

	if ( 0 <= distance && distance < r/3 ) {
		section = 1;
	}
	else if (r/3 <= distance && distance < r/3*2) {
		section = 2;
	}
	else {
		section = 3;
	}

	return section ;
}
void UanMacCw1::setPoint()
{
	//ConstantRandomVariable�϶��� ���밡���� ���
	uint32_t currNode = m_nodeIncrease; //����� ������ġ����
	Vector2D arr[256];
	APNode = splitPoint(arr, currNode); //AP����� ��ȣ�� ��ǥ���� split�ؼ� arr�� ����

	//�Ÿ����
	uint32_t myNode = (uint32_t)m_address.GetAsInt(); //��������� �Ÿ� ���
	while(currNode != APNode)
	{
		double distance;
		if ( currNode != myNode) {
			distance = CalculateDistance(arr[myNode],arr[currNode]);
		}
		else {//������� 
			distance = -1; 
		}
		m_distance[currNode] = make_pair(distance,getSection(arr,currNode,APNode));

		currNode = (currNode+1)%m_nodeMAX;
	}
	// ������� ���� ����� �������� ����
	 sortDistance();
}
void  UanMacCw1::sortDistance() //��������
{
	//Map�� �ִ°� vector�� �־���
	m_sortedDistance.assign(m_distance.begin(),m_distance.end());

	//compare�Լ�
	struct sort_pred {
    bool operator()(const std::pair<int,pair<double,int>> &left, const std::pair<int,pair<double,int>> &right) {
		return left.second.first < right.second.first;
    }
	};
	//����
	std::sort(m_sortedDistance.begin(), m_sortedDistance.end(), sort_pred());
	
	/*
	for ( int i = 0 ; i < m_sortedDistance.size() ; ++i)
	{
		cout << (uint32_t)m_address.GetAsInt() << " :: " << m_sortedDistance[i].first << " :: " << m_sortedDistance[i].second.first << " :: " << m_sortedDistance[i].second.second << " " << '\n';
	}
	*/
}
double UanMacCw1::CalculateDistance (const Vector2D &a, const Vector2D &b)
{
  double dx = b.x - a.x;
  double dy = b.y - a.y;
  double distance = std::sqrt (dx * dx + dy * dy );
  return distance;
}

uint32_t UanMacCw1::findNearestNode(uint32_t section)
{
	for ( int i=0; i < m_sortedDistance.size() ; ++i)
	{
		//�ڱⱸ������ ���� 
		if ( section -1 == m_sortedDistance[i].second.second && m_sortedDistance[i].first != APNode) //
		{
			return m_sortedDistance[i].first;
		}
	}

	return NOT_FOUND;
}
uint32_t UanMacCw1::getNearestNode()
{
	uint32_t section = m_distance[(uint32_t)m_address.GetAsInt()].second;
	uint32_t selectedSection2Node = NOT_FOUND;
	uint32_t selectedSection1Node = NOT_FOUND ;

	if ( section == 1 )
	{
		return NOT_FOUND;
	}
	else if ( section == 2 )
	{
		return findNearestNode(section);
	}
	else
	{
		selectedSection2Node = findNearestNode(section);

		/*���� ����2�� ��尡 ���� ��� ���� 1 Ž��*/
		if ( selectedSection2Node == NOT_FOUND )
		{
			return findNearestNode(section-1);
		}
		else
			return selectedSection2Node;
	}
}
 uint32_t UanMacCw1::getRandomNode()
 {
	uint32_t section = m_distance[(uint32_t)m_address.GetAsInt()].second;
	uint32_t selectedSection2Node = NOT_FOUND;
	uint32_t selectedSection1Node = NOT_FOUND ;

	if ( section == 1 )
	{
		return NOT_FOUND;
	}
	else if ( section == 2 )
	{
		return findRandomNode(section);
	}
	else
	{
		selectedSection2Node = findRandomNode(section);

		/*���� ����2�� ��尡 ���� ��� ���� 1 Ž��*/
		if ( selectedSection2Node == NOT_FOUND )
		{
			 return findRandomNode(section-1);
		}
		else
			return selectedSection2Node;
	}
 }
 uint32_t UanMacCw1::findRandomNode(uint32_t section)
 {
	
	bool flag = true ;
	int *arr = new int[m_sortedDistance.size()];

	for ( int i = 0  ; i < m_sortedDistance.size(); ++i)
	{
		arr[i] = 0 ;
	}

	while ( flag )
	{
		int i = (uint32_t) m_rv->GetValue (0,m_sortedDistance.size());

		if ( arr[i] == 1 )
			continue ;

		arr[i] = 1;
		
		
		
		//�ڱⱸ������ ���� 
		if ( section -1 == m_sortedDistance[i].second.second ) //
		{
			delete [] arr;
			return m_sortedDistance[i].first;
		}

		//�����ȿ� ���°�� üũ
		int cnt = 0 ;
		for ( int j = 0 ; j < m_sortedDistance.size() ; ++j )
		{
			if ( arr[j] == 1 )
			{
				cnt++;
			}
		}

		if ( cnt == m_sortedDistance.size())
		{
			delete [] arr;
			return NOT_FOUND;
		}
	}
	delete [] arr;
	return NOT_FOUND;
 }
 uint32_t UanMacCw1::getFairnessNode()
 {
	uint32_t section = m_distance[(uint32_t)m_address.GetAsInt()].second;
	uint32_t selectedSection2Node = NOT_FOUND;
	uint32_t selectedSection1Node = NOT_FOUND ;

	if ( section == 1 )
	{
		return NOT_FOUND;
	}
	else if ( section == 2 )
	{
		return findFairnessNode(section);
	}
	else
	{
		selectedSection2Node = findFairnessNode(section);

		/*���� ����2�� ��尡 ���� ��� ���� 1 Ž��*/
		if ( selectedSection2Node == NOT_FOUND )
		{
			return findFairnessNode(section-1);
		}
		else
			return selectedSection2Node;
	}
 }
 uint32_t UanMacCw1::findFairnessNode(uint32_t section)
 {
	double NSCV = 0;
	uint32_t index = NOT_FOUND;
	for ( uint32_t i=0; i < m_sortedDistance.size() ; ++i)
	{
		//�ڱⱸ������ ���� 
		if ( section -1 == m_sortedDistance[i].second.second && m_sortedDistance[i].first != APNode) //
		{
			if ( NSCV < m_sortedDistance[i].second.first*dataBytes*(m_exceptCount+1))
			{
				NSCV = m_sortedDistance[i].second.first*dataBytes*(m_exceptCount+1);
				index = i ;
			}
		}
	}

	return index;
 }

Time UanMacCw1::CalculateTime(uint32_t selectedNode)
{
	return Seconds(0);
}

void UanMacCw1::processingSuccess()
{
	//����ī��Ʈ ����
	if ( m_sendTime != Seconds(0) )
	{
		Simulator::Cancel(m_sendEvent);
		m_sendTime = Seconds(0);

		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": sendTimer����\n";
	}

	m_successCount++;
	m_cw = m_reset_cw ;
	m_retryCnt = 0 ;

	//Delay���
	m_SumDelays += Simulator::Now() - m_startPacket.front();
	m_startPacket.clear();


	if ( DEBUG_PRINT > 0)
	{
		cout << "Time " << Simulator::Now ().GetSeconds () <<  " ***** NodeNum : " <<  (uint32_t)m_address.GetAsInt() << "  SuccessCount -> " << m_successCount << " *****\n";
		cout << "Time " << Simulator::Now ().GetSeconds () <<  " ##### NodeNum : " <<  (uint32_t)m_address.GetAsInt() << "  SumDealy -> " << m_SumDelays.GetSeconds() << " *****\n";
	}
	m_state = IDLE;

	//�ٸ� ������ ���� ������ ���� ��������
	if ( m_sleepTime > Simulator::Now() )
	{
		m_navEndEvent = Simulator::Schedule ( m_sleepTime-Simulator::Now() , &UanMacCw1::navTimer, this); // 

		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": NAV�̺�Ʈ ���� " << (Simulator::Now()+m_sleepTime-Simulator::Now()).GetSeconds() << " : NAV����ð�\n";

		m_navTime = (m_sleepTime-Simulator::Now()) + Simulator::Now();
		m_prevState = m_state;
		m_state = NAV;
	}


	if ( m_pktTx.size() > 0 )
	{
		if ( isIDLE() )
		{
			//difsevent ����
			m_state = RUNNING;
			m_difsEvent = Simulator::Schedule (Seconds(m_difs), &UanMacCw1::difsTimer, this); // 2*SlotTime(m_sifs)+ SIFS(m_sifs)
			m_difsTime = Seconds(m_difs) + Simulator::Now();
			if ( DEBUG_PRINT >1)
				cout << "Time " << Simulator::Now().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << ": DIFS�̺�Ʈ ���� " << " Time " <<  m_difsTime.GetSeconds() << "�� DIFS �̺�Ʈ �ߵ�\n";
		}
	}
}
void UanMacCw1::setCtsPacket(int src)
{
	Ptr<Packet> pkt;
	//NAV ����
	if ( m_navTime != Seconds(0) )
	{
		Simulator::Cancel(m_navEndEvent);
		m_navTime = Seconds(0);
	}
	UanHeaderRcPhy ph;

	m_prevState = m_state;
	m_state = RUNNING;

	//��Ŷ ����
	pkt = 0;
	std::pair <Ptr<Packet>, UanAddress > m_p = m_pktTx.front();
	pkt = m_pktTx.front().first; //���� ������ ��Ŷ �߰�
	m_pktTx.front().first = 0 ;
	m_pktTx.pop_front();

	pkt->AddHeader(ph);
	//ctsh��� ����
	UanHeaderRcCts1 tempctsh;

	//CTS��� �����Ͽ� ����ð� �߰�
	tempctsh.SetTimeStamp(Simulator::Now());
	Time duration =  Seconds( (( m_CTS + m_DATA + m_ACK  )*8.0) / m_currentRate ) + Seconds(m_sifs*2) + Seconds(m_maxPropDelay*3) ; // ( CTS +  DATA + ACK )/bps + SIFS * 3
	//��Ŷ�� ������ �ٸ� ���� ������ ����� ���� ���� �־�� �ϴ� �ð��� ����ϱ� ���� ����
	m_sleepTime = duration + Simulator::Now();

	if ( DEBUG_PRINT > 1) {
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << "�� " << src << "�� ������ �޾� " << m_sleepTime.GetSeconds() <<"�� ���ۿϷ�ð� \n";
	}

	//duration ����
	tempctsh.SetDuration(duration); // (typeheader �����½ð� + commonheader�����½ð� + packect �����½ð�)*8*4 + SIFS*3 //8����Ʈ
	tempctsh.SetProtocolNumber(m_pktTxProt.front());
	m_pktTxProt.pop_front();

	pkt->AddHeader(tempctsh);
	//common ����� ����� ����� ������ ��Ŷ Ÿ�� ����
	UanHeaderCommon tempch;
	tempch.SetDest(m_address);
	tempch.SetSrc (m_p.second);
	tempch.SetType (TYPE_CTS);
	pkt->AddHeader(tempch);
	m_transPktTx = pkt ;

}

void UanMacCw1::addExcetionCount(uint32_t section)
{
	if ( section == m_sortedDistance[0].second.second)
	{
		m_exceptCount += 0.1; //������ �ڻ��ΰ� ����ؾ���
	}
}


} // namespace ns3


/*
--�⺻
1. ��ǥ�� string���� �����´�.
2. ��ǥ�� split �Ѵ�.
3. ap�� �������� ������ ������.
4. ��ǥ�� �������� �Ÿ��� ����Ѵ�.
5. ���� ���κ��� �Ÿ��� ����� �������� ������ �Ѵ�.
//��尡 �����ϸ� �ش� ����� ��ȣ�� ���� ����
//�ش��带 �������� �Ÿ��� ���
-------------------------------�Ϸ�-------------------------

6. RTS,CTS�� ���ϰ� �����͸� �����Ҷ� �����ȿ� ��带 �ϳ� �����ؼ� data header�� �߰��Ѵ�.

--3������ ���
2������ ��带 �߰� , 2������ ��尡 1������ ��带 �߰� �ϸ鼭 ��ü �ɸ��� �ð��� ���
--2������ ���
1������ ��� �߰�, ��ü �ɸ��� �ð� ���
--1������ ���
�Ϲ����� ������ �ϸ��

//������ �����ȿ� �ִ³༮ �� ���� �ƴϰ� ,AP�� �ƴ� �� �߿�
6.1 �ִܰŸ�
6.2 ���� 
6.3 ����

7. ���ù��� �����ȿ� ����� ���
7.1 A->AP�� �����µ� �ɸ��� �ð� (RXEnd�� �����ڿ� �ٷ� RxStart�� �� �ִ°� �ּ��� ���)�� ����Ͽ� �ٷ� �̾ ������ ����
7.2
8. �߰��Ǿ��ٴ°� ���� AP�� ���
ù �����͸� �ް� ��ü ������ �ð����� NAV or Ư�� ����� ������ �����
���� ACK��Ŷ�� �����ϴµ� ���� ������ ����� ��ȣ�� �߰��ؼ� ����
ACK�� ���� ���� ����Ƚ�� ���� -> ������ ���� �ڱⲲ �ƴϴ� ������ġ ����

2017-06-30 ��������
1. ���� �������� �����Ϳ� ���� NAV�� �����Ѱ��� �ƴ� �� �������� NAV�� �����Ǿ� �־� �ٸ� ������ NAV���� ����� �ٸ� �ൿ�� �� ����� ����
2. sleep�ð��� �ʹ� �ʰ��Ǿ �����Ǿ����� �ð� ����� ��Ȯ�� �ؾ���
�ؾ��� ��
3. startpacket�� difs�������� �����ؾ��� -�ذ�
4. ������� �༮�� ���� backoff�����ϸ� ������ cw�� 2��� ���� - �ذ�
5. ���� ��ǥ 10���� �����Ͽ� ����� ������

*/