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
	m_reset_cw(10), // 기존 10
	dataBytes(200), // 기존 200
	m_sifs(0.2), // 기존 0.2
	m_difs(2*1.5+0.2), // 기존 2*slottime + sifs;
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

/*Attribute를 설정하면 main의 값을 설정할 수 있음*/
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

	//좌표를 설정
	if (m_pointFlag)
	{
		setPoint();
	}

	if (m_pktTx.size () >= m_queueLimit)
    {
		/* 패킷 오버플로우*/
      return false;
    }

  //패킷 등록
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

		  //IDLE 체크
		if ( !isIDLE() )
		{
			//busy상태
			return true;
		}
		//RUNNING 현재 패킷을 보낼준비하는 단계  
		m_state = RUNNING;

		m_difsEvent = Simulator::Schedule ( Seconds(m_difs), &UanMacCw1::difsTimer, this); // 2*SlotTime(0.2)+ SIFS(0.2)
		m_difsTime = Seconds(m_difs)+Simulator::Now();
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": DIFS이벤트 생성 " << m_difsTime.GetSeconds() << "에 DIFS 이벤트 발동\n";
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
	
	//백오프 중이면 취소하고 현재 Backoff시간을 기록해둠
	if ( m_state == BACKOFF || m_backoffTime != Seconds(0) )
	{
		m_backoff = m_backoffTime - Simulator::Now();
		Simulator::Cancel(m_backOffEvent);
		m_backoffTime = Seconds(0);
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": BACKOFF 이벤트 삭제 BACKOFF 남은시간 : " << m_backoff.GetSeconds() << "\n";
	}
	//상태를 RX로 변경
	m_prevState = m_state ;
	m_state = RX;
	//SIFS중이므로 RX무시
	if ( m_sifsTime != Seconds(0))
	{
		m_state = RUNNING;
		if ( DEBUG_PRINT > 1)
			cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " SIFS 이벤트 중이므로 RxStart 무시 \n";
	}
	//RX상태 이므로 DIFS 이벤트 삭제
	if ( m_difsTime != Seconds(0) )
	{
		if ( DEBUG_PRINT >1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 RX 상태이므로 difs 이벤트 삭제\n";
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
	//RX가 끝났으므로 RUNNING
	m_state = RUNNING;
}
void
UanMacCw1::NotifyRxEndError (void)
{
	if ( DEBUG_PRINT > 1)
		cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " RxEndError \n";

	//EIFS중에 또 실패하는경우 취소하고 다시 만듬
	if (m_eifsEvent.IsRunning ())
    {
		if ( DEBUG_PRINT > 1)
			cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " Eifs 발동 중 인것 취소\n";
      Simulator::Cancel (m_eifsEvent);
    }

	m_prevState = m_state ;
	m_state = EIFS;

	m_eifsTime = Seconds( (m_ACK*8.0)/m_currentRate ) + Seconds(m_sifs) ; // ACK + PHY 전송시간 + SIFS시간 
	m_eifsEvent = Simulator::Schedule (m_eifsTime, &UanMacCw1::eifsTimer, this);
	m_eifsTime = m_eifsTime + Simulator::Now() ;

	if ( DEBUG_PRINT > 1)
		cout << "Time " <<Simulator::Now().GetSeconds() << " Addr " << (uint32_t) m_address.GetAsInt() << " Eifs 이벤트 생성 " << m_eifsTime.GetSeconds() << "에 Eifs 이벤트 발동\n";

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
	/*sendPacket을 하면 발동되는 함수*/
	if ( DEBUG_PRINT >1)
		cout <<  "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() <<  " Tx start " << " Time " <<  Simulator::Now ().GetSeconds () + duration.GetSeconds() << "에 Tx End\n" ;
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

	//TX 끝나면 IDLE상태
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
		cout <<  "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " RxEnd 성공시간\n";

	/*상위계층으로 올리는부분*/
	//예외처리
	if ( m_sifsTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout <<  "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " SIFS중에 패킷을 수신한 패킷 버림\n";

		m_prevState = m_state;
		m_state = RUNNING;
		return ;
	}

	if ( m_state == RX)
	{
		if ( DEBUG_PRINT > 1)
			cout <<  "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " RX중에 패킷을 수신할 수 없음\n";
		NS_FATAL_ERROR ( "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " RX중에 패킷을 수신할 수 없음");
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

	//1. RTS/CTS/DATA/ACK의 헤더를 분리한다.
	
	UanHeaderRcData1 datah;
	UanHeaderRcRts1 rtsh;
	UanHeaderRcCts1 ctsh;
	UanHeaderRcAck1 ackh;
	UanHeaderRcPhy ph;
	Ptr<Packet> dataPkt ;

	//보낼 패킷을 만들고 지연을 계산
	switch(ch.GetType())
	{
	case TYPE_DATA:
		pkt->RemoveHeader(datah);
		pkt->RemoveHeader(ph);

		if ( ch.GetDest() == m_address)
		{
			dataPkt = pkt ; //순수 데이터
			pkt = 0 ;
			pkt = new Packet();
			pkt->AddHeader(ph);
			pkt->AddHeader(datah);
		}
		m_propDelay[ch.GetSrc()] = Simulator::Now() - datah.GetTimeStamp() - Seconds (m_DATA * 8.0 / m_currentRate); // 현재시간 - 받은시간 = (지연시간+패킷전송시간) - 패킷전송시간
		break;

	case TYPE_RTS:
		pkt->RemoveHeader(rtsh);
		if ( ch.GetDest() == m_address)
		{
			pkt->AddHeader(rtsh);
		}
		m_propDelay[ch.GetSrc()] = Simulator::Now() - rtsh.GetTimeStamp() - Seconds (m_RTS * 8.0 / m_currentRate); // 현재시간 - 받은시간 = (지연시간+패킷전송시간) - 패킷전송시간
		break;

	case TYPE_CTS:
		pkt->RemoveHeader(ctsh);
		pkt->RemoveHeader(ph);
		pkt = 0;

		if ( ch.GetDest() == m_address)
		{
			pkt = m_pktTx.front().first; //순수 데이터 패킷 추가
			m_pktTx.front().first = 0 ;
			m_pktTx.pop_front();
			m_pktTxProt.pop_front();


			pkt->AddHeader(ph);
			pkt->AddHeader(ctsh);
		}
		m_propDelay[ch.GetSrc()] = Simulator::Now() - ctsh.GetTimeStamp() - Seconds (m_CTS * 8.0 / m_currentRate); // 현재시간 - 받은시간 = (지연시간+패킷전송시간) - 패킷전송시간
		break;

	case TYPE_ACK:
		pkt->RemoveHeader(ackh);
		if ( ch.GetDest() == m_address)
		{
			pkt->AddHeader(ackh);
		}
		m_propDelay[ch.GetSrc()] = Simulator::Now() - ackh.GetTimeStamp() - Seconds (m_ACK * 8.0 / m_currentRate); // 현재시간 - 받은시간 = (지연시간+패킷전송시간) - 패킷전송시간
		break;

	default : 
		if ( DEBUG_PRINT > 1) {
			cout << "Unknown packet type " << ch.GetType () << " received at node " << (uint32_t)m_address.GetAsInt()<< "\n";
		}
		NS_FATAL_ERROR ("Unknown packet type " << ch.GetType () << " received at node " << (uint32_t)m_address.GetAsInt());
	}

	

	if ( DEBUG_PRINT > 2)
	{
		cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " src " << (uint32_t)ch.GetSrc().GetAsInt() << "-> dest " << (uint32_t)m_address.GetAsInt() << " " << m_propDelay[ch.GetSrc()].GetSeconds() << " propDelay시간 \n";
		cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " src " << (uint32_t)ch.GetSrc().GetAsInt() << "-> dest " << (uint32_t)m_address.GetAsInt() << " " << Seconds (bytes * 8.0 / m_currentRate).GetSeconds() << " 패킷전송시간\n ";
		cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " src " << (uint32_t)ch.GetSrc().GetAsInt() << "-> dest " << (uint32_t)m_address.GetAsInt() << " " << m_propDelay[ch.GetSrc()].GetSeconds() + Seconds (bytes * 8.0 / m_currentRate).GetSeconds() << " 지연+패킷전송시간\n ";
	}

	//내 패킷인가 아닌가를 구분한다.
	if ( ch.GetDest() == m_address )
	{
		/*지연시간 계산*/
		pkt->AddHeader(ch);
		m_transPktTx = pkt ;

		/*
		--수신--
		0. 송신자의 timerEvent 삭제해서 데이터 전송을 완료했다고 알림
		SIFS event를 발생시킨다.
		--SIFS--
		1. 새로운 common헤더를 만들어 목적지 도착지 주소를 변경 한다.
		*/
		
		/* timer 이벤트 해제 */
		if ( m_sendTime != Seconds(0) )
		{
			if ( DEBUG_PRINT > 1)
				cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " m_sendEvent 삭제 \n";
			Simulator::Cancel(m_sendEvent);
			m_sendTime = Seconds(0);
		}
		m_prevState = m_state;
		m_state = RUNNING;

		switch (ch.GetType ())
		{
			/* 
			--SIFS--
			0. common헤더의 m_type을 rts->cts, cts->data, data->ack로 설정한다.
			1. data/rts/cts/ack header 생성
			2.  패킷 데이터 전송시간을 어떻게 구하느냐?
			맨처음 duration 설정 packet->duration = (RTS / BasicRate) + (CTS / BasicRate) + payload_tx_time(packet->soucAddress, packet->destAddress1) + (ACK / BasicRate) + SIFS * 3;
			rts reply_packet->duration = act_node->receive_packet.duration - (RTS / act_node->receive_packet.dataRate + SIFS); // - RTS전송속도 + SIFS시간
			cts reply_packet->duration = act_node->receive_packet.duration - (CTS / act_node->receive_packet.dataRate + SIFS);
			data reply_packet->duration = act_node->receive_packet.duration - (payload_tx_time(reply_packet->soucAddress, reply_packet->destAddress1) + SIFS);
			3. data/rts/cts/ack의 헤더가 가지는 duration 필드 값을 조절한 뒤 헤더를 만든다.
			--송신--
			2. timer Event(현재시간+ SIFS시간 내가 보낼 데이터패킷의 길이(rts/cts/data/ack)+ 가는 지연시간 + 상대가 보낼 데이터패킷의 길이+ 오는지연시간 )를 설정한다. //timerevent가 발생하면 데이터전송 실패임
			//timerEvent의 시간을 계산하는 방법은?!
			uint32_t ctsBytes = ch.GetSerializedSize () + pkt->GetSize ();
			double m_currentRate = m_phy->GetMode (m_currentRate).GetDataRateBps ();
			m_learnedProp = Simulator::Now () - ctsg.GetTxTimeStamp () - Seconds (ctsBytes * 8.0 / m_currentRate);
			4. sendPacket(패킷) 시작
			//타이머는 rts/cts/data/ack의 sendPacket앞에서 발동해야함
			*/

		case TYPE_DATA:
			{
			/*내것의 패킷이지만 다른노드로부터 패킷을 받아 현재 NAV상태인 경우*/
			if ( m_navTime != Seconds(0) && m_waitFlag == false)
			{
				if ( DEBUG_PRINT >1)
					cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " 현재 NAV중 DATA패킷 받았지만 SIFS 이벤트 발동 무시 \n";
				return;
			}
			//상위계층으로 전송
			m_forwardUpCb (dataPkt, ch.GetSrc ());

			//패킷을 비우고 새롭게 만듬 순수데이터 제거용

			
			m_transPktTx = 0 ;
			m_transPktTx = new Packet();
			m_transPktTx->AddHeader(ph);
			m_transPktTx->AddHeader(datah);
			m_transPktTx->AddHeader(ch);

			if ( datah.GetSelectedNode() == NOT_FOUND )
			{
				// 그냥 평소대로 진행
				if ( m_navTime != Seconds(0) )
				{
					Simulator::Cancel(m_navEndEvent);
					m_navTime = Seconds(0);
					if ( DEBUG_PRINT > 1) {
						cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 연속적인 데이터 전체 수신완료 " << "NAV 삭제 \n";
					}

					m_transPktTx = m_savePktTx;
				}
				if ( DEBUG_PRINT > 1) {
					cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " navTime : " << m_navTime.GetSeconds() <<"\n";
				}

				
			}
			else
			{
				//AP의 경우 다른 데이터가 연속으로 올수 있는지 확인 후 NAV의 들어가야함
				//데이터를 연속적으로 받고 다시 SIFS를 동작시키면됨
				if ( m_selectedNodes.size() == 0 )
				{
					m_savePktTx = m_transPktTx;
				}
				m_selectedNodes.push_back(datah.GetSelectedNode());

				Time tempNavTime = datah.GetArrivalTime(); //임시로 설정

				if ( m_navTime != Seconds(0)  ) // 기존의 NAV < 현재의 NAV인 경우 큰 시간으로 덮어씀
				{
					Time nextNavTime = tempNavTime + Simulator::Now();
					if ( m_navTime < nextNavTime )
					{
						Simulator::Cancel(m_navEndEvent);
						m_navTime = tempNavTime ;
						m_navEndEvent = Simulator::Schedule(m_navTime, &UanMacCw1::navTimer, this);
						m_navTime = Simulator::Now () + m_navTime;

						if ( DEBUG_PRINT >1)
							cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 기존의 NAV 삭제 후 NAV이벤트 생성 " << m_navTime.GetSeconds() << " : NAV종료시간\n";
					}
				}
				else
				{
					m_navEndEvent = Simulator::Schedule (tempNavTime, &UanMacCw1::navTimer, this); // 

					if ( DEBUG_PRINT > 1)
						cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": NAV이벤트 생성 " << (Simulator::Now ()+tempNavTime).GetSeconds() << " : NAV종료시간\n";
			
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
			// 늦게온 노드가 dest로 설정되는 아이러니한 상황 발생

			m_waitFlag = false ;
			/*SIFS를 통해 보내지는 패킷과 포트번호*/
			m_sifsEvent =  Simulator::Schedule (Seconds(m_sifs), &UanMacCw1::sifsTimer, this); // ACK 전송 SIFS
			if ( DEBUG_PRINT > 1)
				cout << "Time " << (Simulator::Now()).GetSeconds() << " Addr "  << (uint32_t)m_address.GetAsInt() << " SIFSEvent 이벤트 생성 " << " Time "<< (Simulator::Now()+Seconds(m_sifs)).GetSeconds() << "에 SIFSEvent 이벤트 발동\n";

			m_sifsTime = Simulator::Now() + Seconds(m_sifs);
			}
				break;
		case TYPE_RTS:
			{
			/*내것의 패킷이지만 다른노드로부터 패킷을 받아 현재 NAV상태인 경우*/
			if ( m_navTime != Seconds(0))
			{
				if ( DEBUG_PRINT > 1)
					cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " 현재 NAV중 RTS패킷 받았지만 SIFS 이벤트 발동 무시 \n";
				return;
			}
			m_sifsEvent =  Simulator::Schedule (Seconds(m_sifs), &UanMacCw1::sifsTimer, this); // CTS 전송 SIFS
			if ( DEBUG_PRINT > 1)
				cout << "Time " << (Simulator::Now()).GetSeconds() << " Addr "  << (uint32_t)m_address.GetAsInt() << " SIFSEvent 이벤트 생성 " << " Time " << (Simulator::Now()+Seconds(m_sifs)).GetSeconds() << "에 SIFSEvent 이벤트 발동\n";
			m_sifsTime = Simulator::Now() + Seconds(m_sifs);
			}
			break;
		case TYPE_CTS:
			{
			/*내것의 패킷이지만 다른노드로부터 패킷을 받아 현재 NAV상태인 경우*/
			if ( m_navTime != Seconds(0))
			{
				if ( DEBUG_PRINT > 1)
					cout <<  "Time " << Simulator::Now().GetSeconds() << " Addr " << (uint32_t)m_address.GetAsInt() << " 현재 NAV중 CTS패킷 받았지만 SIFS 이벤트 발동 무시 \n";
				return;
			}
			m_sifsEvent =  Simulator::Schedule (Seconds(m_sifs), &UanMacCw1::sifsTimer, this); // DATA 전송 SIFS
			if ( DEBUG_PRINT > 1)
				cout << "Time " << (Simulator::Now()).GetSeconds() << " Addr "  << (uint32_t)m_address.GetAsInt() << " SIFSEvent 이벤트 생성 " << " Time "<< (Simulator::Now()+Seconds(m_sifs)).GetSeconds() << "에 SIFSEvent 이벤트 발동\n";
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
	else //내께 아니니깐 NAV 이벤트로
	{
		/*
		1. 자신의 상태가 NAV인 경우
		1.1 현재 NAV의 duration값과 비교하여 이전 nav시간 <  현재 nav 시간인 경우
		이전 nav시간 = 현재 nav시간을 넣어서 갱신함

		2. 자신의 상태가 NAV가 아닌경우
		2.1 RTS/CTS/DATA/ACK의 헤더를 분리한다.
		2.2 NAVEvent(현재시간 + duration) 를 설정한다.
		*/

		if ( ch.GetType() == TYPE_ACK && (ackh.GetFirstNode() == (uint32_t)m_address.GetAsInt() || ackh.GetSecondNode() == (uint32_t)m_address.GetAsInt()))
		{
			/* 연속적인 데이터 전송방법 적용 */
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
				cout << "Time " << Simulator::Now().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " sendTimer 작동중이므로 NAV이벤트 생성 무시 받은 패킷버림\n";
			}
			m_prevState = m_state ;
			m_state = IDLE;
			return ;
		}
		
		//ex) RTS + CTS + DATA + ACK -> CTS+ DATA + ACK 

		/* 2017-05-31 수정사항
		NAV의 지연시간은 maxdistance/speed 시간을 더해줘야함
		*/

		Time propDelay = Seconds(0);
		Time tempNavTime = Seconds(0) ;

		switch(ch.GetType())
		{
		case TYPE_RTS:
			tempNavTime = rtsh.GetDuration() - Seconds(m_RTS * 8.0 / m_currentRate); // - Seconds(m_maxPropDelay)  ; // 
			//propDelay = m_propDelay[ch.GetDest()]+m_propDelay[ch.GetSrc()] + m_propDelay[ch.GetDest()];// CTS지연, DATA지연, ACK지연
			//propDelay = Seconds(m_maxPropDelay)  + Seconds(m_maxPropDelay) + Seconds(m_maxPropDelay) ;
			break;
		case TYPE_CTS:
			tempNavTime = ctsh.GetDuration() - Seconds(m_CTS * 8.0 / m_currentRate) - Seconds(m_maxPropDelay);
			//propDelay = m_propDelay[ch.GetSrc()]+m_propDelay[ch.GetDest()]; // DATA지연, ACK지연
			//propDelay = Seconds(m_maxPropDelay)  + Seconds(m_maxPropDelay)  ;
			break;
		case TYPE_DATA:
			/* 연속적인 데이터 전송방법 적용 */
			if (datah.GetSelectedNode() == (uint32_t)m_address.GetAsInt())
			{
				setCtsPacket(ch.GetSrc().GetAsInt());
				// 일정시간 뒤 SIFS돌입
				// 도착시간 - datah받는데 걸린 지연시간
				Time arrival = datah.GetArrivalTime() - (Simulator::Now() - datah.GetTimeStamp()); //arrival 시간을 더 정확하게 계산해야함
				// 선택된 노드의 거리보다 AP와의 거리보다 긴 경우 발생함
				if ( arrival < 0 ) {
					arrival = Seconds(0) ;
				}
				if ( DEBUG_PRINT > 1 ) {
					cout << "datah.GetArrivalTime() : " << datah.GetArrivalTime() << " Simulator::Now() : " << Simulator::Now() << " datah.GetTimeStamp() : " << datah.GetTimeStamp()<< "\n";  
					cout << "(Simulator::Now() - datah.GetTimeStamp()) : " << (Simulator::Now() - datah.GetTimeStamp()) << '\n';
				}

				/*SIFS를 통해 보내지는 패킷과 포트번호*/
				m_sifsEvent =  Simulator::Schedule (arrival, &UanMacCw1::sifsTimer, this); // ACK 전송 SIFS
				if ( DEBUG_PRINT > 1)
					cout << "Time " << (Simulator::Now()).GetSeconds() << " Addr "  << (uint32_t)m_address.GetAsInt() << ": 기존의 NAV 삭제 후 연속적인 데이터 전송을 위한 SIFS 생성 " << (Simulator::Now()+arrival).GetSeconds() << "에 SIFSEvent 이벤트 발동\n";
				m_sifsTime = Simulator::Now() + arrival;
				return ;
			}

			tempNavTime = datah.GetDuration() - Seconds (m_DATA * 8.0 / m_currentRate) - Seconds(m_maxPropDelay);
			//propDelay = m_propDelay[ch.GetDest()]; // ACK지연
			//propDelay = Seconds(m_maxPropDelay)  ;
			break;
		case TYPE_ACK:
			//가중치 추가
			addExcetionCount((uint32_t)m_distance[ch.GetSrc().GetAsInt()].second);
			tempNavTime = ackh.GetDuration() - Seconds (m_ACK * 8.0 / m_currentRate) - Seconds(m_maxPropDelay);
			break;
		}

		
		tempNavTime += propDelay; // duration + 지연

		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " 현재 duration + 지연 -> " << (Simulator::Now()+tempNavTime).GetSeconds() << "\n" ;

		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << " 이전 duration + 지연 -> " << m_navTime.GetSeconds() << "\n" ;


		if ( m_navTime != Seconds(0)  ) // 기존의 NAV < 현재의 NAV인 경우 큰 시간으로 덮어씀
		{
			Time nextNavTime = tempNavTime + Simulator::Now();
			if ( m_navTime < nextNavTime )
			{
				Simulator::Cancel(m_navEndEvent);
				m_navTime = tempNavTime ;
				m_navEndEvent = Simulator::Schedule(m_navTime, &UanMacCw1::navTimer, this);
				m_navTime = Simulator::Now () + m_navTime;

				if ( DEBUG_PRINT >1)
					cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 기존의 NAV 삭제 후 NAV이벤트 생성 " << m_navTime.GetSeconds() << " : NAV종료시간\n";
			}

			m_prevState = m_state;
			m_state = NAV;
		}
		else
		{
			m_navEndEvent = Simulator::Schedule (tempNavTime, &UanMacCw1::navTimer, this); // 

			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": NAV이벤트 생성 " << (Simulator::Now ()+tempNavTime).GetSeconds() << " : NAV종료시간\n";
			
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
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": NAV이벤트 발동\n";

	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 TX 상태이므로 NAV 발동 에러\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 TX 상태이므로 NAV 발동 에러");
	}
	
	if ( m_sendTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 sendTimer 상태이므로 NAV 이벤트 무시\n";
		m_navTime = Seconds(0);
		return ;
	}

	if ( m_state == RX)
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 RX 상태이므로 NAV 이벤트 무시\n";
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

			//difsevent 생성
			m_difsEvent = Simulator::Schedule (Seconds(m_difs), &UanMacCw1::difsTimer, this); // 2*SlotTime(m_sifs)+ SIFS(m_sifs)
			m_difsTime = Simulator::Now()+ Seconds(m_difs);
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": DIFS 이벤트 생성" << " Time " <<m_difsTime.GetSeconds() << "에 DIFS 이벤트 발동\n";
			//NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": DIFS 이벤트 생성" << " Time " <<(Simulator::Now ()+Seconds(2*m_slotTime.GetSeconds())+ Seconds(m_sifs)).GetSeconds() << "에 DIFS 이벤트 발동");
		}
	}

}
void UanMacCw1::difsTimer()
{
	/*
		  1. backoff event(현재시간 + cw(랜덤) *슬롯타임 )를 발생한다.
		  2. backoff event이 발동하면 sendRtsEvent를 설정한다.
	*/
	if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " difs 이벤트 발동\n";

	m_difsTime = Seconds(0);
	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 TX 상태이므로 difs 발동 에러\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 TX 상태이므로 difs 발동 에러");
	}
	if ( m_state == RX )
	{
		if ( DEBUG_PRINT >1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 RX 상태이므로 difs 이벤트 무시\n";
		Simulator::Cancel(m_difsEvent);
		return ;
	}

	if ( m_navTime != Seconds(0) )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 NAV 상태이므로 difs 이벤트 무시\n";
		Simulator::Cancel(m_difsEvent);
		return ;
	}



	if ( m_sendTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 sendTimer 상태이므로 difs 이벤트 무시\n";
		return ;
	}

	if (m_startPacket.empty() )
	{
		m_startPacket.push_back(Simulator::Now());
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 패킷 헤더에 설정 -> " << Simulator::Now().GetSeconds() << "\n";
		
	}
	

	if ( m_pktTx.size() > 0 )
	{

		
		m_prevState = m_state;
		m_state = BACKOFF;

		if (m_backoff != Seconds(0))
		{
			m_backoffTime = m_backoff;
			if ( DEBUG_PRINT >1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << ": 남은 BackoffTime -> " << m_backoff.GetSeconds() << " BACKOFF이벤트 생성 " << " Time " << (Simulator::Now() + m_backoffTime).GetSeconds() << "에 backoff 발동\n";
		}
		else
		{// bc 값이 0이라면 랜덤값(0~CW) * slot_time 시간으로 이벤트 시간 설정 

			uint32_t cw = (uint32_t) m_rv->GetValue (0,m_cw);
			m_backoffTime = Seconds ((double)(cw) * m_slotTime.GetSeconds ());
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " cw-> " << cw <<" : BACKOFF이벤트 생성 " << " Time " << (Simulator::Now() + m_backoffTime).GetSeconds() << "에 backoff 발동\n";
		}

		//backoffevent 생성
		m_backOffEvent = Simulator::Schedule (m_backoffTime, &UanMacCw1::backOffTimer, this); // 2*SlotTime(m_sifs)+ SIFS(m_sifs)
		m_backoffTime += Simulator::Now(); // 나중에 남은 백오프 시간을 계산하기 위해서

	}

}
void UanMacCw1::sendTimer()
{
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt()<< ": sendTimer이벤트 발동\n";

	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 TX 상태이므로 sendTimer 발동 에러\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 TX 상태이므로 sendTimer 발동 에러\n");
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
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << ": DIFS이벤트 생성 " << m_difsTime.GetSeconds() << "에 DIFS 이벤트 발동\n";
		}
		
	}

	//RTS의 경우만 실패처리
	if ( m_sendType == RTS)
	{
		m_failCount++;
		m_cw = m_cw * 2;
		m_retryCnt ++;
		m_sendType = UNKNOWN;
		if ( DEBUG_PRINT > 0)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 전송실패!! failCount -> " << m_failCount  << " m_retryCnt -> " << m_retryCnt << " cw-> " << m_cw << "\n";

		//3번 실패 후 초기화
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
				cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 해당 패킷 3번 연속 전송실패!! 패킷 버리고 cw초기화\n";
		}
	}
}
void UanMacCw1::backOffTimer()
{
	/*
	1. backoffTimer : backoff가 완료하여 전송을 시작하는 단계
	2. RTS를 보내고 CTS를 받는데까지 걸리는 시간을 설정해야함
	3. sendPacket 동작
	*/
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " BackOffEvent 발동\n";
	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 TX 상태이므로 backoffTimer 발동 에러\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 TX 상태이므로 backoffTimer 발동 에러");
	}
	if ( m_state == RX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 RX 상태이므로 backoffTimer 발동 에러\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 RX 상태이므로 backoffTimer 발동 에러");
	}
	if ( m_sifsTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 SIFS 상태이므로 backoffTimer 발동 에러\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 SIFS 상태이므로 backoffTimer 발동 에러");
	}

	m_backoff = Seconds(0) ;
	m_sendType = RTS;

	// 1.rts헤더를 만들어 현재시간을 등록 -> 나중에 지연시간 계산용임
	// 2. common헤더를 만들어 src , dest를 설정한다.

	//큐에서 패킷을 가져옴
	std::pair <Ptr<Packet>, UanAddress > m_p = m_pktTx.front();
	Ptr<Packet> transPkt = new Packet();

	//common 헤더를 만들어 출발지 도착지 패킷 타입 설정
	UanHeaderCommon header;
	header.SetDest(m_p.second);
	header.SetSrc (m_address);
	header.SetType (TYPE_RTS);
	
	//RTS헤더 생성하여 현재시간 추가
	UanHeaderRcRts1 rtsHeader ; 
	rtsHeader.SetTimeStamp(Simulator::Now());

	//피지컬 계층 시간을 위해 피지컬 헤더 추가
	UanHeaderRcPhy ph;

	// data(200) + phy(17) + data(27) + common(3) -> 247
		// phy(17) + rts(17) + common(3) -> 37
		// phy(17) + cts(11) + common(3) -> 31
		// phy(17) + ack(14) + common(3) -> 34

	//Time duration =  Seconds( ((m_RTS + m_CTS + m_DATA + m_ACK  )*8.0) / m_currentRate ) + Seconds(m_sifs*3) ; // (RTS + CTS +  DATA + ACK )/bps + SIFS * 3
	/* 2017-06-22 duration에 m_propDealy추가*/	
	Time duration =  Seconds( ((m_RTS + m_CTS + m_DATA + m_ACK  )*8.0) / m_currentRate ) + Seconds(m_sifs*3) + Seconds(m_maxPropDelay*4) ; // (RTS + CTS +  DATA + ACK )/bps + SIFS * 3
	
	//패킷을 보내고 다른 노드와 동일한 출발을 위해 잠들어 있어야 하는 시간을 계산하기 위한 변수
	m_sleepTime = duration + Simulator::Now();

	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " " << m_sleepTime.GetSeconds() <<"에 전송완료시간 \n";

	//duration 설정
	rtsHeader.SetDuration(duration); // (typeheader 보내는시간 + commonheader보내는시간 + packect 보내는시간)*8*4 + SIFS*3 //8바이트
	rtsHeader.SetProtocolNumber(m_pktTxProt.front());

	//패킷에 헤더 추가
	transPkt->AddHeader (ph);
	transPkt->AddHeader (rtsHeader);
	transPkt->AddHeader (header);

	//sendtimer 설정
	uint32_t Bytes = transPkt->GetSize ();
	
	Time currentPropDelay ;
	//지연이 계산되기 전에는 max 지연시간으로 계산하다가 서로간에 통신하면서 지연을 알아가면 알아낸 지연으로 대체
	if ( m_propDelay[(uint32_t)m_p.second.GetAsInt()] != Seconds(0) )
	{
		//NS_LOG_DEBUG (m_propDelay[(uint32_t)m_p.second.GetAsInt()].GetSeconds());
		m_sendTime = (m_propDelay[(uint32_t)m_p.second.GetAsInt()]) + (m_propDelay[(uint32_t)m_p.second.GetAsInt()]) + Seconds(m_sifs) + Seconds(m_RTS*8.0/m_currentRate)+ Seconds(m_CTS*8.0/m_currentRate) + Seconds(m_mifs); // 지연시간*2 + SIFS + 패킷전송시간*2
		currentPropDelay = m_propDelay[(uint32_t)m_p.second.GetAsInt()];
	}
	else
	{
		m_sendTime = Seconds(m_maxPropDelay*2) + Seconds(m_sifs) + Seconds(m_RTS*8.0/m_currentRate)+ Seconds(m_CTS*8.0/m_currentRate) + Seconds(m_mifs); // 지연시간*2 + SIFS + 패킷전송시간*2
		currentPropDelay = Seconds(m_maxPropDelay);
	}

	
	m_sendEvent = Simulator::Schedule (m_sendTime, &UanMacCw1::sendTimer, this); // RTS보내는지연+CTS받는지연+ 보내는 패킷전송속도 + 받는 패킷전송속도 + SIFS
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)m_p.second.GetAsInt() << ": sendTimer이벤트 생성 " << (Simulator::Now() +m_sendTime).GetSeconds() << "에 sendTimer이벤트 발동\n";
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)m_p.second.GetAsInt() << ": RTS 패킷 전송 " << (Simulator::Now() +Seconds(m_RTS*8.0/m_currentRate) +currentPropDelay).GetSeconds() << "에 RTS 패킷 도착\n";

	m_sendTime = Simulator::Now() + m_sendTime;

	//if ( DEBUG_PRINT > 1)
		//	cout <<  "Time " << Simulator::Now().GetSeconds() <<   " Addr " << (uint32_t)m_address.GetAsInt() << " packet size->" << transPkt->GetSize() << "\n";

	//패킷전송
	m_phy->SendPacket (transPkt,m_pktTxProt.front());
	//delete해줌
	transPkt = 0 ;
	m_backoffTime = Seconds(0);
}
void UanMacCw1::sifsTimer()
{
	if ( DEBUG_PRINT > 1)
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " SIFSTimer 이벤트 발동\n"  ;

	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 TX 상태이므로 sifs 발동 에러\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 TX 상태이므로 sifs 발동 에러");
	}
	if ( m_state == RX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 RX 상태이므로 sifs 발동 에러\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 RX 상태이므로 sifs 발동 에러");
	}
	if ( m_state == NAV )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 NAV 상태이므로 sifs 발동 에러\n";
		NS_FATAL_ERROR ("Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 현재 NAV 상태이므로 sifs 발동 에러");
	}

	//common헤더 부분을 가져옴
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

	//1. RTS/CTS/DATA/ACK의 헤더를 분리한다.
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
			transCtsh.SetDuration(rtsh.GetDuration() - Seconds (m_RTS * 8.0 / m_currentRate) - Seconds(m_sifs) -Seconds(m_maxPropDelay));  // 현재 SIFS까지 끝난 상황이기 때문에 RTS라도 SIFS시간을 빼줘야함
			transCtsh.SetProtocolNumber(rtsh.GetProtocolNumber());
			protNum = rtsh.GetProtocolNumber();
			
			m_transPktTx->AddHeader(transCtsh);
			m_sendTime = m_propDelay[ch.GetSrc()] + m_propDelay[ch.GetSrc()] + Seconds( ((m_CTS*8.0)/m_currentRate)) + Seconds( ((m_DATA*8.0)/m_currentRate)) + Seconds(m_sifs) + Seconds(m_mifs); // 지연시간*2 + SIFS + 패킷전송시간*2 + mifs

			m_sendEvent = Simulator::Schedule (m_sendTime, &UanMacCw1::sendTimer, this); // RTS보내는지연+CTS받는지연+ 보내는 패킷전송속도 + 받는 패킷전송속도 + SIFS
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << ": sendTimer이벤트 생성 " << (Simulator::Now() + m_sendTime).GetSeconds() << "에 sendTimer이벤트 발동\n";
			m_sendTime = Simulator::Now() + m_sendTime;

			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << ": CTS 패킷 전송 "<< (Simulator::Now()+m_propDelay[ch.GetSrc()] + Seconds( ((m_CTS*8.0)/m_currentRate))).GetSeconds() << "에 CTS 패킷 도착\n";
			}
			break;
		case TYPE_CTS:
			{

			m_transPktTx->RemoveHeader(ctsh);
			m_sendType = DATA;
			transCh.SetType(TYPE_DATA);

			/* 연속적인 데이터 전송알고리즘 추가 부분 */
			uint32_t selectedNode = NOT_FOUND;

			if (m_startPacket.empty() )
			{
				m_startPacket.push_back(Simulator::Now());
				if ( DEBUG_PRINT > 1)
					cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << " 패킷 헤더에 설정 -> " << Simulator::Now().GetSeconds() << "\n";
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

			//현재 노드와 AP까지의 지연시간 + 데이터 전송시간
			//Ack는 AP가 데이터를 여러개 받을 수 있기에 시간에서 제외시켜 놓음
			Time arrivalTime = m_propDelay[ch.GetSrc()] + Seconds(m_DATA*8.0/m_currentRate) ;

			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << " arrivalTime->  " << (Simulator::Now() +arrivalTime).GetSeconds() << "에 데이터 패킷 도착\n";

			transDatah.SetArrivalTime(arrivalTime);
			transDatah.SetSelectedNode(selectedNode);
			transDatah.SetTimeStamp(Simulator::Now());
			transDatah.SetDuration(ctsh.GetDuration() - Seconds (m_CTS * 8.0 / m_currentRate) - Seconds(m_sifs) - Seconds(m_maxPropDelay));  // data 패킷은 아직안보냈기때문에 byte에서 200만큼의 데이터 패킷 수를 빼줘야함
			transDatah.SetProtocolNumber(ctsh.GetProtocolNumber());
			protNum = ctsh.GetProtocolNumber();
			m_transPktTx->AddHeader(transDatah);

			m_sendTime = m_propDelay[ch.GetSrc()] + m_propDelay[ch.GetSrc()] + Seconds(m_DATA*8.0/m_currentRate)+ Seconds(m_ACK*8.0/m_currentRate) + Seconds(m_sifs)+ Seconds(m_mifs); // 지연시간*2 + SIFS + 패킷전송시간*2 + mifs

			for ( int i = 1 ; i < m_distance[(uint32_t)m_address.GetAsInt()].second; ++i)
			{
				m_sendTime += m_sendTime; //sendTime시간 계산을 더 정확하게 해야함
			}
			m_sendEvent = Simulator::Schedule (m_sendTime, &UanMacCw1::sendTimer, this); // RTS보내는지연+CTS받는지연+ 보내는 패킷전송속도 + 받는 패킷전송속도 + SIFS
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << ": sendTimer이벤트 생성 " << (Simulator::Now() +m_sendTime).GetSeconds() << "에 sendTimer이벤트 발동\n";
			m_sendTime = Simulator::Now() + m_sendTime;
			if ( DEBUG_PRINT > 1)
				cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << ": DATA 패킷 전송 "<< (Simulator::Now()+  m_propDelay[ch.GetSrc()] + Seconds(m_DATA*8.0/m_currentRate)).GetSeconds() << "에 DATA 패킷 도착\n";
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
				cout << "Time " << Simulator::Now ().GetSeconds () << " src " << (uint32_t)m_address.GetAsInt() << "-> dest " << (uint32_t)ch.GetSrc().GetAsInt() << ": ACK 패킷 전송 "<< (Simulator::Now() +Seconds(m_ACK*8.0/m_currentRate) +m_propDelay[ch.GetSrc()]).GetSeconds() << "에 ACK 패킷 도착\n";
			}

			m_selectedNodes.clear();
			break;
	}
	

	/* 전송 */
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
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 현재 sendTimer 이벤트 중 DIFS이벤트 무시 " << (m_sendTime).GetSeconds() << "에 sendTimer 이벤트 발동\n";
		return false ;
	}

	if ( m_navTime != Seconds(0) || m_state == NAV)
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 현재 navTimer 이벤트 중 DIFS이벤트 무시 " << (m_navTime).GetSeconds() << "에 navTimer 이벤트 발동\n";
		return false ;
	}

	if ( m_sifsTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 현재 sifs 이벤트 중 DIFS이벤트 무시 " << (m_sifsTime).GetSeconds() << "에 sifs 이벤트 발동\n";
		return false ;
	}

	if ( m_difsTime != Seconds(0))
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 현재 difs 이벤트 중 DIFS이벤트 무시 " << (m_difsTime).GetSeconds() << "에 difs 이벤트 발동\n";
		return false ;
	}

	if ( m_backoffTime  != Seconds(0) || m_state == BACKOFF)
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 현재 backOff 이벤트 중 DIFS이벤트 무시 " << (m_backoffTime).GetSeconds() << "에 backoff 이벤트 발동\n";
		return false ;
	}

	if ( m_eifsTime  != Seconds(0) || m_state == EIFS)
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 현재 EIFS 이벤트 중 DIFS이벤트 무시 " << (m_eifsTime).GetSeconds() << "에 EIFS 이벤트 발동\n";
		return false ;
	}

	if ( m_state == RX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 현재 RX 이벤트 중 DIFS이벤트 무시\n " ;
		return false ;
	}

	if ( m_state == TX )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 현재 TX 이벤트 중 DIFS이벤트 무시\n " ;
		return false ;
	}

	
	if ( m_state == RUNNING )
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": 현재 RUNNING 이벤트 중 DIFS이벤트 무시\n " ;
		return false ;
	}
	
	return true ;
}
void UanMacCw1::eifsTimer()
{
	if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": EIFS 이벤트 발동\n";
	
	m_eifsTime = Seconds(0);
	
	if ( m_state == RX) // eifs중에 rx인경우 rx상태를 유지
	{
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": Rx상태 이므로 EIFS 이벤트 발동 취소\n";
		return ;
	}

	m_state = IDLE;

	if ( isIDLE() )
	{
		m_state = RUNNING;
		m_difsEvent = Simulator::Schedule (Seconds(m_difs), &UanMacCw1::difsTimer, this); // 2*SlotTime(m_sifs)+ SIFS(m_sifs)
		m_difsTime = Seconds(m_difs) + Simulator::Now();
		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": DIFS이벤트 생성 " << m_difsTime.GetSeconds() << "에 DIFS 이벤트 발동\n";
	}
}

uint32_t UanMacCw1::splitPoint(Vector2D arr[], uint32_t count)
{
	uint32_t len = m_point.length();
	uint32_t apX;
	uint32_t apY;
	uint32_t nodeNum; //전체노드의 수
	bool nodeFlag= true; // 첫번째 전체노드의 수를 split용 flag
	uint32_t apFlag = 0; // ap좌표를 가져오기 위한 flag
	bool xyFlag = true ; // true -> x 좌표, false - > y좌표

	std::string s="";
	int x,y;
	
	//스플릿
	for ( uint32_t i = 0 ; i < len ;++i)
	{
		if ( m_point[i] == '#') //#이 스플릿의 단위
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
	//마지막에 AP좌표를 넣어줌 실제로 AP는 마지막 address를 사용하기때문에
	arr[count].x = apX;
	arr[count].y = apY;

	return count ; // AP의 위치 리턴
	
}
uint32_t UanMacCw1::getSection(Vector2D arr[], int currNode, int APNode)
{
	//AP노드와 다른노드와의 거리를 계산하여 구간을 설정
	double distance = CalculateDistance(arr[APNode],arr[currNode]);  //ap와 다른 노드의 거리 계산

	uint32_t section;
	double r = (m_boundary - arr[APNode].x)*sqrt(2) ; //1:1:sqrt(2) 피타고라스의 정리

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
	//ConstantRandomVariable일때만 적용가능한 방식
	uint32_t currNode = m_nodeIncrease; //노드의 시작위치지정
	Vector2D arr[256];
	APNode = splitPoint(arr, currNode); //AP노드의 번호와 좌표들을 split해서 arr에 넣음

	//거리계산
	uint32_t myNode = (uint32_t)m_address.GetAsInt(); //현재노드와의 거리 계산
	while(currNode != APNode)
	{
		double distance;
		if ( currNode != myNode) {
			distance = CalculateDistance(arr[myNode],arr[currNode]);
		}
		else {//같은경우 
			distance = -1; 
		}
		m_distance[currNode] = make_pair(distance,getSection(arr,currNode,APNode));

		currNode = (currNode+1)%m_nodeMAX;
	}
	// 현재노드와 가장 가까운 노드순으로 정렬
	 sortDistance();
}
void  UanMacCw1::sortDistance() //오름차순
{
	//Map에 있는걸 vector에 넣어줌
	m_sortedDistance.assign(m_distance.begin(),m_distance.end());

	//compare함수
	struct sort_pred {
    bool operator()(const std::pair<int,pair<double,int>> &left, const std::pair<int,pair<double,int>> &right) {
		return left.second.first < right.second.first;
    }
	};
	//정렬
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
		//자기구간보다 안쪽 
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

		/*만약 구간2의 노드가 없는 경우 구간 1 탐색*/
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

		/*만약 구간2의 노드가 없는 경우 구간 1 탐색*/
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
		
		
		
		//자기구간보다 안쪽 
		if ( section -1 == m_sortedDistance[i].second.second ) //
		{
			delete [] arr;
			return m_sortedDistance[i].first;
		}

		//구간안에 없는경우 체크
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

		/*만약 구간2의 노드가 없는 경우 구간 1 탐색*/
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
		//자기구간보다 안쪽 
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
	//성공카운트 증가
	if ( m_sendTime != Seconds(0) )
	{
		Simulator::Cancel(m_sendEvent);
		m_sendTime = Seconds(0);

		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": sendTimer삭제\n";
	}

	m_successCount++;
	m_cw = m_reset_cw ;
	m_retryCnt = 0 ;

	//Delay계산
	m_SumDelays += Simulator::Now() - m_startPacket.front();
	m_startPacket.clear();


	if ( DEBUG_PRINT > 0)
	{
		cout << "Time " << Simulator::Now ().GetSeconds () <<  " ***** NodeNum : " <<  (uint32_t)m_address.GetAsInt() << "  SuccessCount -> " << m_successCount << " *****\n";
		cout << "Time " << Simulator::Now ().GetSeconds () <<  " ##### NodeNum : " <<  (uint32_t)m_address.GetAsInt() << "  SumDealy -> " << m_SumDelays.GetSeconds() << " *****\n";
	}
	m_state = IDLE;

	//다른 노드와의 같은 시작을 위해 잠들어있음
	if ( m_sleepTime > Simulator::Now() )
	{
		m_navEndEvent = Simulator::Schedule ( m_sleepTime-Simulator::Now() , &UanMacCw1::navTimer, this); // 

		if ( DEBUG_PRINT > 1)
			cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " << (uint32_t)m_address.GetAsInt() << ": NAV이벤트 생성 " << (Simulator::Now()+m_sleepTime-Simulator::Now()).GetSeconds() << " : NAV종료시간\n";

		m_navTime = (m_sleepTime-Simulator::Now()) + Simulator::Now();
		m_prevState = m_state;
		m_state = NAV;
	}


	if ( m_pktTx.size() > 0 )
	{
		if ( isIDLE() )
		{
			//difsevent 생성
			m_state = RUNNING;
			m_difsEvent = Simulator::Schedule (Seconds(m_difs), &UanMacCw1::difsTimer, this); // 2*SlotTime(m_sifs)+ SIFS(m_sifs)
			m_difsTime = Seconds(m_difs) + Simulator::Now();
			if ( DEBUG_PRINT >1)
				cout << "Time " << Simulator::Now().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << ": DIFS이벤트 생성 " << " Time " <<  m_difsTime.GetSeconds() << "에 DIFS 이벤트 발동\n";
		}
	}
}
void UanMacCw1::setCtsPacket(int src)
{
	Ptr<Packet> pkt;
	//NAV 삭제
	if ( m_navTime != Seconds(0) )
	{
		Simulator::Cancel(m_navEndEvent);
		m_navTime = Seconds(0);
	}
	UanHeaderRcPhy ph;

	m_prevState = m_state;
	m_state = RUNNING;

	//패킷 생성
	pkt = 0;
	std::pair <Ptr<Packet>, UanAddress > m_p = m_pktTx.front();
	pkt = m_pktTx.front().first; //순수 데이터 패킷 추가
	m_pktTx.front().first = 0 ;
	m_pktTx.pop_front();

	pkt->AddHeader(ph);
	//ctsh헤더 생성
	UanHeaderRcCts1 tempctsh;

	//CTS헤더 생성하여 현재시간 추가
	tempctsh.SetTimeStamp(Simulator::Now());
	Time duration =  Seconds( (( m_CTS + m_DATA + m_ACK  )*8.0) / m_currentRate ) + Seconds(m_sifs*2) + Seconds(m_maxPropDelay*3) ; // ( CTS +  DATA + ACK )/bps + SIFS * 3
	//패킷을 보내고 다른 노드와 동일한 출발을 위해 잠들어 있어야 하는 시간을 계산하기 위한 변수
	m_sleepTime = duration + Simulator::Now();

	if ( DEBUG_PRINT > 1) {
		cout << "Time " << Simulator::Now ().GetSeconds () << " Addr " <<(uint32_t)m_address.GetAsInt() << "는 " << src << "의 도움을 받아 " << m_sleepTime.GetSeconds() <<"에 전송완료시간 \n";
	}

	//duration 설정
	tempctsh.SetDuration(duration); // (typeheader 보내는시간 + commonheader보내는시간 + packect 보내는시간)*8*4 + SIFS*3 //8바이트
	tempctsh.SetProtocolNumber(m_pktTxProt.front());
	m_pktTxProt.pop_front();

	pkt->AddHeader(tempctsh);
	//common 헤더를 만들어 출발지 도착지 패킷 타입 설정
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
		m_exceptCount += 0.1; //원래는 코사인값 계산해야함
	}
}


} // namespace ns3


/*
--기본
1. 좌표를 string으로 가져온다.
2. 좌표를 split 한다.
3. ap를 기준으로 구간을 나눈다.
4. 좌표를 바탕으로 거리를 계산한다.
5. 현재 노드로부터 거리가 가까운 노드순으로 정렬을 한다.
//노드가 증가하면 해당 노드의 번호도 같이 증가
//해당노드를 기준으로 거리를 계산
-------------------------------완료-------------------------

6. RTS,CTS를 다하고 데이터를 전송할때 구간안에 노드를 하나 선택해서 data header에 추가한다.

--3구간의 경우
2구간의 노드를 추가 , 2구간의 노드가 1구간의 노드를 추가 하면서 전체 걸리는 시간을 계산
--2구간의 경우
1구간의 노드 추가, 전체 걸리는 시간 계산
--1구간의 경우
일반적인 전송을 하면됨

//나보다 구간안에 있는녀석 중 내가 아니고 ,AP도 아닌 것 중에
6.1 최단거리
6.2 랜덤 
6.3 공평성

7. 선택받은 구간안에 노드의 경우
7.1 A->AP로 보내는데 걸리는 시간 (RXEnd가 끝난뒤에 바로 RxStart할 수 있는게 최선의 방법)을 계산하여 바로 이어서 데이터 전송
7.2
8. 추가되었다는걸 알은 AP의 경우
첫 데이터를 받고 전체 데이터 시간까지 NAV or 특정 노드의 수신을 대기함
그후 ACK패킷을 전송하는데 받은 데이터 노드의 번호를 추가해서 보냄
ACK를 받은 노드는 성공횟수 증가 -> 공평성의 경우는 자기께 아니니 공평가중치 증가

2017-06-30 개선사항
1. 현재 연속적인 데이터에 대한 NAV를 설정한것이 아닌 한 데이터의 NAV만 설정되어 있어 다른 노드들이 NAV에서 깨어나서 다른 행동을 할 우려가 있음
2. sleep시간이 너무 초과되어서 설정되어있음 시간 계산을 정확히 해야함
해야할 일
3. startpacket을 difs시작으로 변경해야함 -해결
4. 도움받은 녀석은 현재 backoff존재하면 삭제후 cw값 2배로 증가 - 해결
5. 랜덤 좌표 10개를 선정하여 평균을 내야함

*/