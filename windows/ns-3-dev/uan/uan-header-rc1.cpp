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


#include "uan-header-rc1.h"

#include <set>

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (UanHeaderRcData1);
NS_OBJECT_ENSURE_REGISTERED (UanHeaderRcRts1);
NS_OBJECT_ENSURE_REGISTERED (UanHeaderRcCts1);
NS_OBJECT_ENSURE_REGISTERED (UanHeaderRcAck1);
NS_OBJECT_ENSURE_REGISTERED (UanHeaderRcPhy);



UanHeaderRcData1::UanHeaderRcData1 ()
	:  Header (),
	m_timeStamp (Seconds (0))
    
{
}

UanHeaderRcData1::UanHeaderRcData1 (Time timeStamp)
  : Header (),
    m_timeStamp (timeStamp)
    
{

}

UanHeaderRcData1::~UanHeaderRcData1 ()
{
}

TypeId
UanHeaderRcData1::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::UanHeaderRcData1")
    .SetParent<Header> ()
    .AddConstructor<UanHeaderRcData1> ()
  ;
  return tid;
}

void
UanHeaderRcData1::SetTimeStamp (Time ts)
{
  m_timeStamp = ts;
}



Time
UanHeaderRcData1::GetTimeStamp (void) const
{
	return m_timeStamp;
}

void
UanHeaderRcData1::SetDuration (Time duration)
{
  m_duration = duration;
}



Time
UanHeaderRcData1::GetDuration (void) const
{
	return m_duration;
}

void
UanHeaderRcData1::SetArrivalTime (Time a)
{
  m_arrivalTime = a;
}
Time
UanHeaderRcData1::GetArrivalTime (void) const
{
	return m_arrivalTime;
}

void 
UanHeaderRcData1::SetProtocolNumber(uint16_t p) 
{
	m_protocolNumber = p;
}
uint16_t
UanHeaderRcData1::GetProtocolNumber(void) const
{
	return m_protocolNumber;
}

void 
UanHeaderRcData1::SetSelectedNode(uint32_t s) 
{
	m_selectedNode = s;
}
uint32_t
UanHeaderRcData1::GetSelectedNode(void) const
{
	return m_selectedNode;
}

uint32_t
UanHeaderRcData1::GetSerializedSize (void) const
{
  return 4+4+2+4+4+8+1;
}

void
UanHeaderRcData1::Serialize (Buffer::Iterator start) const
{
	start.WriteU32 ((uint32_t)(m_timeStamp.GetSeconds () * 100000.0 + 0.5));
	 start.WriteU32 ((uint32_t)(m_duration.GetSeconds () * 100000.0 + 0.5));
	 start.WriteU32((uint32_t)(m_arrivalTime.GetSeconds() * 100000.0+0.5));
	 start.WriteU16 (m_protocolNumber);
	 start.WriteU32(m_selectedNode);
	 start.WriteU64 (m_MAC_hdr1);
	 start.WriteU8 (m_MAC_hdr2);
}
uint32_t
UanHeaderRcData1::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator rbuf = start;

 
  m_timeStamp = Seconds ( ((double) rbuf.ReadU32 ()) / 100000.0 );
    m_duration = Seconds ( ((double) rbuf.ReadU32 ()) / 100000.0 );
    m_arrivalTime = Seconds ( ((double) rbuf.ReadU32 ()) / 100000.0 );

	/*추가부분*/
	m_protocolNumber = rbuf.ReadU16 ();
	m_selectedNode = rbuf.ReadU32 ();
	m_MAC_hdr1 = rbuf.ReadU64 ();
	m_MAC_hdr2 = rbuf.ReadU8 ();
  return rbuf.GetDistanceFrom (start);
}

void
UanHeaderRcData1::Print (std::ostream &os) const
{
 os << " Time Stamp=" << m_timeStamp.GetSeconds ();
}

TypeId
UanHeaderRcData1::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}




UanHeaderRcRts1::UanHeaderRcRts1 ()
  : Header (),
    m_timeStamp (Seconds(0))
  
{

}

UanHeaderRcRts1::UanHeaderRcRts1 (Time timeStamp)
  :  Header (),
    m_timeStamp (timeStamp)

{

}

UanHeaderRcRts1::~UanHeaderRcRts1 ()
{

}

TypeId
UanHeaderRcRts1::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::UanHeaderRcRts1")
    .SetParent<Header> ()
    .AddConstructor<UanHeaderRcRts1> ()
  ;
  return tid;

}

void
UanHeaderRcRts1::SetTimeStamp (Time ts)
{
  m_timeStamp = ts;
}



Time
UanHeaderRcRts1::GetTimeStamp (void) const
{
	return m_timeStamp;
}

void
UanHeaderRcRts1::SetDuration (Time duration)
{
  m_duration = duration;
}



Time
UanHeaderRcRts1::GetDuration (void) const
{
	return m_duration;
}
/*추가부분*/
void 
UanHeaderRcRts1::SetProtocolNumber(uint16_t p) 
{
	m_protocolNumber = p;
}
/*추가부분*/
uint16_t
UanHeaderRcRts1::GetProtocolNumber(void) const
{
	return m_protocolNumber;
}



uint32_t
UanHeaderRcRts1::GetSerializedSize (void) const
{
  return 4+4+2+4+1+1+1;// +1+1+1;
}

void
UanHeaderRcRts1::Serialize (Buffer::Iterator start) const
{
 
  start.WriteU32 ((uint32_t)(m_timeStamp.GetSeconds () * 100000.0 + 0.5));
	 start.WriteU32 ((uint32_t)(m_duration.GetSeconds () * 100000.0 + 0.5));
	  /*추가부분*/
	 start.WriteU16 (m_protocolNumber);

	 start.WriteU32 (m_MAC_hdr1);
	 start.WriteU8 (m_MAC_hdr2);
	 start.WriteU8 (m_MAC_hdr3);
	 start.WriteU8 (m_MAC_hdr4);

	 //start.WriteU8 (m_MAC_hdr5);
	 //start.WriteU8 (m_MAC_hdr6);
	 //start.WriteU8 (m_MAC_hdr7);
}

uint32_t
UanHeaderRcRts1::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator rbuf = start;
 
  m_timeStamp = Seconds ( ((double) rbuf.ReadU32 ()) / 100000.0 );
    m_duration = Seconds ( ((double) rbuf.ReadU32 ()) / 100000.0 );
	/*추가부분*/
	m_protocolNumber = rbuf.ReadU16 ();
	m_MAC_hdr1 = rbuf.ReadU32 ();
	m_MAC_hdr2 = rbuf.ReadU8();
	m_MAC_hdr3 = rbuf.ReadU8 ();
	m_MAC_hdr4 = rbuf.ReadU8 ();

	//m_MAC_hdr5 = rbuf.ReadU8();
	//m_MAC_hdr6 = rbuf.ReadU8 ();
	//m_MAC_hdr7 = rbuf.ReadU8 ();
  return rbuf.GetDistanceFrom (start);
}

void
UanHeaderRcRts1::Print (std::ostream &os) const
{
  os << " Time Stamp=" << m_timeStamp.GetSeconds ();
}

TypeId
UanHeaderRcRts1::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}




UanHeaderRcCts1::UanHeaderRcCts1 ()
  :  Header (),
    m_timeStamp (Seconds(0))
    
{

}

UanHeaderRcCts1::UanHeaderRcCts1 (Time ts)
  :  Header (),
    m_timeStamp (ts)
{

}

UanHeaderRcCts1::~UanHeaderRcCts1 ()
{

}
TypeId
UanHeaderRcCts1::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::UanHeaderRcCts1")
    .SetParent<Header> ()
    .AddConstructor<UanHeaderRcRts1> ()
  ;
  return tid;

}

void
UanHeaderRcCts1::SetTimeStamp (Time ts)
{
  m_timeStamp = ts;
}



Time
UanHeaderRcCts1::GetTimeStamp (void) const
{
	return m_timeStamp;
}

void
UanHeaderRcCts1::SetDuration (Time duration)
{
  m_duration = duration;
}



Time
UanHeaderRcCts1::GetDuration (void) const
{
	return m_duration;
}
/*추가부분*/
void 
UanHeaderRcCts1::SetProtocolNumber(uint16_t p) 
{
	m_protocolNumber = p;
}
/*추가부분*/
uint16_t
UanHeaderRcCts1::GetProtocolNumber(void) const
{
	return m_protocolNumber;
}

uint32_t
UanHeaderRcCts1::GetSerializedSize (void) const
{
  return 4+4+2+1;// 11
}

void
UanHeaderRcCts1::Serialize (Buffer::Iterator start) const
{
 
   start.WriteU32 ((uint32_t)(m_timeStamp.GetSeconds () * 100000.0 + 0.5));
	 start.WriteU32 ((uint32_t)(m_duration.GetSeconds () * 100000.0 + 0.5));
	  /*추가부분*/
	 start.WriteU16 (m_protocolNumber);
	 start.WriteU8 (m_MAC_hdr1);

	 //start.WriteU64 (m_MAC_hdr2);
	 //start.WriteU8 (m_MAC_hdr3);
}

uint32_t
UanHeaderRcCts1::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator rbuf = start;
 
  m_timeStamp = Seconds ( ((double) rbuf.ReadU32 ()) / 100000.0 );
    m_duration = Seconds ( ((double) rbuf.ReadU32 ()) / 100000.0 );
	/*추가부분*/
	m_protocolNumber = rbuf.ReadU16 ();
	m_MAC_hdr1 = rbuf.ReadU8 ();

	//m_MAC_hdr2 = rbuf.ReadU64 ();
	//m_MAC_hdr3 = rbuf.ReadU8 ();
  return rbuf.GetDistanceFrom (start);
}

void
UanHeaderRcCts1::Print (std::ostream &os) const
{
  os << " Time Stamp=" << m_timeStamp.GetSeconds ();
}

TypeId
UanHeaderRcCts1::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

///////////////////////////////////////////
///////////////////////////////////////////
///////////////////////////////////////////


UanHeaderRcAck1::UanHeaderRcAck1 ()
  :  Header (),
    m_timeStamp (Seconds(0))
{
}

UanHeaderRcAck1::~UanHeaderRcAck1 ()
{
 
}
UanHeaderRcAck1::UanHeaderRcAck1 (Time ts)
  :  Header (),
    m_timeStamp (ts)
{
}
TypeId
UanHeaderRcAck1::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::UanHeaderRcAck1")
    .SetParent<Header> ()
    .AddConstructor<UanHeaderRcAck1> ()
  ;
  return tid;
}

void
UanHeaderRcAck1::SetTimeStamp (Time ts)
{
  m_timeStamp = ts;
}



Time
UanHeaderRcAck1::GetTimeStamp (void) const
{
	return m_timeStamp;
}

void
UanHeaderRcAck1::SetDuration (Time duration)
{
  m_duration = duration;
}



Time
UanHeaderRcAck1::GetDuration (void) const
{
	return m_duration;
}
/*추가부분*/
void 
UanHeaderRcAck1::SetProtocolNumber(uint16_t p) 
{
	m_protocolNumber = p;
}
/*추가부분*/
uint16_t
UanHeaderRcAck1::GetProtocolNumber(void) const
{
	return m_protocolNumber;
}
uint16_t
UanHeaderRcAck1::GetFirstNode(void) const
{
	return m_firstNode;
}
void
UanHeaderRcAck1::SetFirstNode(uint16_t firstNode) 
{
	this->m_firstNode = firstNode;
}
uint16_t
UanHeaderRcAck1::GetSecondNode(void) const
{
	return m_secondNode;
}
void 
UanHeaderRcAck1::SetSecondNode(uint16_t secondNode) 
{
	this->m_secondNode = secondNode;
}
uint32_t
UanHeaderRcAck1::GetSerializedSize (void) const
{
  return 4+4+2+2+2 ;// 14
}

void
UanHeaderRcAck1::Serialize (Buffer::Iterator start) const
{
 
 start.WriteU32 ((uint32_t)(m_timeStamp.GetSeconds () * 100000.0 + 0.5));
	 start.WriteU32 ((uint32_t)(m_duration.GetSeconds () * 100000.0 + 0.5));
	  /*추가부분*/
	 start.WriteU16 (m_protocolNumber);
	 start.WriteU16 (m_firstNode);
	 start.WriteU16 (m_secondNode);

	 //start.WriteU64 (m_MAC_hdr2);
	 //start.WriteU8 (m_MAC_hdr3);
}

uint32_t
UanHeaderRcAck1::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator rbuf = start;
 
 m_timeStamp = Seconds ( ((double) rbuf.ReadU32 ()) / 100000.0 );
    m_duration = Seconds ( ((double) rbuf.ReadU32 ()) / 100000.0 );
	/*추가부분*/
	m_protocolNumber = rbuf.ReadU16 ();
	m_firstNode = rbuf.ReadU16 ();
	m_secondNode = rbuf.ReadU16 ();

	//m_MAC_hdr2 = rbuf.ReadU64 ();
	//m_MAC_hdr3 = rbuf.ReadU8 ();

  return rbuf.GetDistanceFrom (start);
}

void
UanHeaderRcAck1::Print (std::ostream &os) const
{
  os << " Time Stamp=" << m_timeStamp.GetSeconds ();
}

TypeId
UanHeaderRcAck1::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

//////////////////
/////////////////////////////////////
///////////////////////////////////////////////////////


UanHeaderRcPhy::UanHeaderRcPhy ()
{
}

UanHeaderRcPhy::~UanHeaderRcPhy ()
{
 
}

TypeId
UanHeaderRcPhy::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::UanHeaderRcPhy")
    .SetParent<Header> ()
    .AddConstructor<UanHeaderRcPhy> ()
  ;
  return tid;
}


uint32_t
UanHeaderRcPhy::GetSerializedSize (void) const
{
  return 8 + 8 + 1 ;
}

void
UanHeaderRcPhy::Serialize (Buffer::Iterator start) const
{
	 start.WriteU64 (m_PHY_hdr1);
  start.WriteU64 (m_PHY_hdr2);
  start.WriteU8 (m_PHY_hdr3);

}

uint32_t
UanHeaderRcPhy::Deserialize (Buffer::Iterator start)
{


  Buffer::Iterator rbuf = start;

  m_PHY_hdr1 = rbuf.ReadU64 ();
  m_PHY_hdr2 = rbuf.ReadU64 ();
  m_PHY_hdr3 = rbuf.ReadU8 ();
  
  return rbuf.GetDistanceFrom (start);
}

void
UanHeaderRcPhy::Print (std::ostream &os) const
{
}

TypeId
UanHeaderRcPhy::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

} // namespace ns3
