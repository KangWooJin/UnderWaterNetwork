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


#ifndef UAN_HEADER_RC_H
#define UAN_HEADER_RC_H

#include "ns3/header.h"
#include "ns3/nstime.h"
#include "ns3/uan-address.h"

#include <set>

namespace ns3 {




/**
 * \class UanHeaderRcData1
 *
 * \brief Extra data header information
 *
 * Adds prop. delay measure, and frame number info to
 * transmitted data packet
 */
class UanHeaderRcData1 : public Header
{
public:
  UanHeaderRcData1 ();
  /**
   * \param frameNum Data frame # of reservation being transmitted
   * \param propDelay  Measured propagation delay found in handshaking
   * \note Prop. delay is transmitted with 16 bits and ms accuracy
   */
  UanHeaderRcData1 (Time timeStamp);
  virtual ~UanHeaderRcData1 ();

  static TypeId GetTypeId (void);

  void SetTimeStamp (Time timeStamp);
  Time GetTimeStamp (void) const;
  void SetDuration (Time du);
  Time GetDuration (void) const;
  uint16_t GetProtocolNumber(void) const;
  void SetProtocolNumber(uint16_t p) ;
   void SetArrivalTime (Time a);
  Time GetArrivalTime (void) const;
  uint32_t GetSelectedNode(void) const;
  void SetSelectedNode(uint32_t n) ;
  

  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;

private:
  Time m_timeStamp;
  Time m_duration;
  uint16_t m_protocolNumber;
  Time m_arrivalTime;
  uint32_t m_selectedNode;
  
  uint64_t m_MAC_hdr1;
  uint8_t m_MAC_hdr2;
};


/**
 * \class UanHeaderRcData1
 *
 * \brief Extra data header information
 *
 * Adds prop. delay measure, and frame number info to
 * transmitted data packet
 */


/**
 * \class UanHeaderRcRts1
 *
 * \brief RTS header
 *
 * Contains frame #, retry #, # frames, length, and timestamp
 */
class UanHeaderRcRts1 : public Header
{
public:
  UanHeaderRcRts1 ();
  /**
   * \param frameNo Reservation frame #
   * \param retryNo Retry # of RTS packet
   * \param noFrames # of data frames in reservation
   * \param length # of bytes (including headers) in data
   * \param ts RTS TX timestamp
   * \note Timestamp is serialized into 32 bits with ms accuracy
   */
  UanHeaderRcRts1 (Time ts);
  virtual ~UanHeaderRcRts1 ();

  static TypeId GetTypeId (void);

   void SetTimeStamp (Time timeStamp);
  Time GetTimeStamp (void) const;
  void SetDuration (Time du);
  Time GetDuration (void) const;
  uint16_t GetProtocolNumber(void) const;
  void SetProtocolNumber(uint16_t p) ;
  
  
  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;

private:
  Time m_timeStamp;
  Time m_duration;
  uint16_t m_protocolNumber;

  uint32_t m_MAC_hdr1;
  uint8_t m_MAC_hdr2;
  uint8_t m_MAC_hdr3;
  uint8_t m_MAC_hdr4;
  
  //uint8_t m_MAC_hdr5;
  //uint8_t m_MAC_hdr6;
  //uint8_t m_MAC_hdr7;
};

/**
 * \class UanHeaderRcCts1Global1
 *
 * \brief Cycle broadcast information for
 */



/**
 * \class UanHeaderRcCts1
 *
 * \brief CTS header
 *
 * Includes RTS RX time, CTS TX time, delay until TX, RTS blocking period,
 * RTS tx period, rate #, and retry rate #
 */

class UanHeaderRcCts1 : public Header
{
public:
  UanHeaderRcCts1 ();
  /**
   * \param frameNo Resrvation frame # being cleared
   * \param retryNo Retry # of received RTS packet
   * \param rtsTs RX time of RTS packet at gateway
   * \param delay Delay until transmission
   * \param addr Destination of CTS packet
   * \note Times are serialized, with ms precission, into 32 bit fields.
   */
  UanHeaderRcCts1 (Time Ts);
  virtual ~UanHeaderRcCts1 ();

  static TypeId GetTypeId (void);

    void SetTimeStamp (Time timeStamp);
  Time GetTimeStamp (void) const;
  void SetDuration (Time du);
  Time GetDuration (void) const;
  uint16_t GetProtocolNumber(void) const;
  void SetProtocolNumber(uint16_t p) ;
  
  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;

private:
  Time m_timeStamp;
  Time m_duration;
  uint16_t m_protocolNumber;

  uint8_t m_MAC_hdr1;

  //uint64_t m_MAC_hdr2;
  //uint8_t m_MAC_hdr3;
};

/**
 * \class UanHeaderRcAck1
 * \brief Header used for ACK packets by protocol ns3::UanMacRc
 */
class UanHeaderRcAck1 : public Header
{
public:
  UanHeaderRcAck1 ();
  UanHeaderRcAck1 (Time Ts);
  virtual ~UanHeaderRcAck1 ();

  static TypeId GetTypeId (void);

  void SetTimeStamp (Time timeStamp);
  Time GetTimeStamp (void) const;
  void SetDuration (Time du);
  Time GetDuration (void) const;
  uint16_t GetProtocolNumber(void) const;
  void SetProtocolNumber(uint16_t p) ;
  uint16_t GetFirstNode(void) const;
  void SetFirstNode(uint16_t firstNode) ;
  uint16_t GetSecondNode(void) const;
  void SetSecondNode(uint16_t secondNode) ;

  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;

private:
  Time m_timeStamp;
  Time m_duration;
  uint16_t m_protocolNumber;

  uint16_t m_firstNode;
  uint16_t m_secondNode;

  //uint64_t m_MAC_hdr2;
  //uint8_t m_MAC_hdr3;
};

/**
 * \class UanHeaderRcAck1
 * \brief Header used for ACK packets by protocol ns3::UanMacRc
 */
class UanHeaderRcPhy : public Header
{
public:
  UanHeaderRcPhy ();
  virtual ~UanHeaderRcPhy ();

  static TypeId GetTypeId (void);

  /**
   * \param frameNo Frame # of reservation being acknowledged
   */


  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;

  

private:

	 uint64_t m_PHY_hdr1;
	 uint64_t m_PHY_hdr2;
	 uint8_t m_PHY_hdr3;

};

}

#endif /* UAN_HEADER_RC_H */
