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

#ifndef UAN_MAC_CW_H
#define UAN_MAC_CW_H

#include "ns3/uan-mac.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/uan-phy.h"
#include "ns3/uan-tx-mode.h"
#include "ns3/uan-address.h"
#include "ns3/random-variable-stream.h"
#include "uan-header-rc1.h"
#include "ns3/string.h"
#include <string>

 #include <map> 
 #include <set> 
 #include <algorithm> 
 #include <functional> 

#include <iostream>
using namespace std;


#define DEBUG_PRINT  2 //Lv0 : ����� �޼��� ��� ����	Lv1 : ����� ���� Ƚ�� ��� Lv2 : ����� �޼��� �κ� ���		Lv3 : ����� �޼��� ��ü ���
#define TRANSMODE 1 // Lv0 : �Ϲ�		Lv1 : ����		 Lv2 : ����		 Lv3 : ����+���������

namespace ns3 {

/**
 * \class UanMacCw1
 * \brief CW-MAC A MAC protocol similar in idea to the 802.11 DCF with constant backoff window
 *
 * For more information on this MAC protocol, see:
 * Parrish, N.; Tracy, L.; Roy, S.; Arabshahi, P.; Fox, W.,
 * "System Design Considerations for Undersea Networks: Link and Multiple Access Protocols,"
 * Selected Areas in Communications, IEEE Journal on , vol.26, no.9, pp.1720-1730, December 2008
 */

uint32_t NOT_FOUND = 999;

class UanMacCw1 : public UanMac,
                 public UanPhyListener
{
public:
  enum {
    TYPE_DATA, TYPE_RTS, TYPE_CTS, TYPE_ACK
  };
  	/*�츮�� �߰��Ѱ�*/
  typedef enum {
    IDLE, CCABUSY, RUNNING, TX, RX, RTSSENT, DATATX, BACKOFF, NAV, EIFS, DIFS
  } State;
  /*�츮�� �߰��Ѱ�*/
  typedef enum {
    RTS,CTS,DATA,ACK,UNKNOWN
  } SendType;
  
  UanMacCw1 ();
  virtual ~UanMacCw1 ();
  static TypeId GetTypeId (void);

  /**
   * \param cw Contention window size
   */
  virtual void SetCw (uint32_t cw);
  /**
   * \param duration Slot time duration
   */
  virtual void SetSlotTime (Time duration);
  /**
   * \returns Contention window size
   */
  virtual uint32_t GetCw (void);
  /**
   * \returns slot time duration
   */
  virtual Time GetSlotTime (void);

  // Inherited methods
  virtual Address GetAddress ();
  virtual void SetAddress (UanAddress addr);
  virtual bool Enqueue (Ptr<Packet> pkt, const Address &dest, uint16_t protocolNumber);
  virtual void SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress&> cb);
  virtual void AttachPhy (Ptr<UanPhy> phy);
  virtual Address GetBroadcast (void) const;
  virtual void Clear (void);

  // PHY listeners
  /// Function called by UanPhy object to notify of packet reception
  virtual void NotifyRxStart (void);
  /// Function called by UanPhy object to notify of packet received successfully
  virtual void NotifyRxEndOk (void);
  /// Function called by UanPhy object to notify of packet received in error
  virtual void NotifyRxEndError (void);
  /// Function called by UanPhy object to notify of channel sensed busy
  virtual void NotifyCcaStart (void);
  /// Function called by UanPhy object to notify of channel no longer sensed busy
  virtual void NotifyCcaEnd (void);
  /// Function called by UanPhy object to notify of outgoing transmission start
  virtual void NotifyTxStart (Time duration);

 /**
  * Assign a fixed random variable stream number to the random variables
  * used by this model.  Return the number of streams (possibly zero) that
  * have been assigned.
  *
  * \param stream first stream index to use
  * \return the number of stream indices assigned by this model
  */
  int64_t AssignStreams (int64_t stream);

private:

  Callback <void, Ptr<Packet>, const UanAddress& > m_forwardUpCb;
  UanAddress m_address;
  Ptr<UanPhy> m_phy;
  TracedCallback<Ptr<const Packet>, UanTxMode > m_rxLogger;
  TracedCallback<Ptr<const Packet>, uint16_t  > m_enqueueLogger;
  TracedCallback<Ptr<const Packet>, uint16_t  > m_dequeueLogger;

  // Mac parameters
  uint32_t m_cw;
  Time m_slotTime;
  double m_sifs; // sifs�� �ð� 0.02��
  double m_difs; // difs�� �ð� slotTime*2+sifs
  uint32_t m_ACK; //ACK��Ŷ�� ũ�� 31
  uint32_t m_RTS; //RTS��Ŷ�� ũ�� 37
  uint32_t m_CTS; //CTS��Ŷ�� ũ�� 31
  uint32_t m_DATA; //DATA��Ŷ�� ũ�� 247
  // State variables
  Time m_sendTime; // SendTimer�� �ð��� �����ϴ� ����
  Time m_navTime; // NavTimer�� �ð��� �����ϴ� ����
  Time m_backoffTime; // BackoffTimer�� �ð��� �����ϴ� ����
  Time m_difsTime; // DifsTimer�� �ð��� �����ϴ� ����
  Time m_sifsTime ; // sifsTimer�� �ð��� �����ϴ� ����
  Time m_eifsTime ; // eifsTimer�� �ð��� �����ϴ� ����
  Time m_backoff ; // ����� �� �ߴܵǾ� ���� backoff�ð�
  Time m_sleepTime; // �����͸� �����ϰ� ���� NAV�ð� ����ȭ��
  Time m_SumDelays; // maxProp�� �����Ͽ��⿡ �� ������ ���� �ִ� �ð�
  list<Time> m_startPacket; // Delay ���� DIFS�� �ߵ��� �ð��� ����صּ� success

  uint32_t m_reset_cw; // 3���� �����ϰ� �ٽ� �ʱ�ȭ�Ǵ� cw�� �� 
  uint32_t dataBytes ; //���� Data ũ�� 

  bool m_pointFlag; // ó�� ��� ��ǥ�� �ѹ��� �޾ƿ��� ���� flag
  bool m_waitFlag; // �������� �����͸� �޾� ���̱� ���� flag
  std::string m_point; // ��ǥ�� �����ϴ� ����
  std::map<uint32_t,pair<double,uint32_t>> m_distance; // Key ����ȣ Value first -> distance second -> section
  vector<pair<uint32_t,pair<double,uint32_t>>> m_sortedDistance; // �Ÿ������� ���ĵǾ� �ִ� vector
  double m_exceptCount; // fairness ����ġ

  std::list<std::pair <Ptr<Packet>, UanAddress >> m_pktTx; // ��Ŷ����� list
  std::list<uint16_t> m_pktTxProt; // port����� list
  uint32_t m_queueLimit; //ť�� ���� ���� 10��
  std::map<UanAddress, Time> m_propDelay; // Key ����ȣ value �����ð�
  vector<uint32_t> m_selectedNodes;
  Ptr<Packet> m_transPktTx; //�����Ҷ� ���
  uint16_t m_transPktTxProt;  //�����Ҷ� ���
  Ptr<Packet> m_savePktTx;

  EventId m_sendEvent; //
  EventId m_txEndEvent;
  EventId m_backOffEvent;
  EventId m_difsEvent;
  EventId m_timerEvent;
  EventId m_navEndEvent;
  EventId m_sifsEvent;
  EventId m_eifsEvent;
  
  uint32_t m_currentRate; // datarate �� 1500
  double m_maxPropDelay ; //�ִ� �����ð� 
  double m_mifs ; //0.001
  void backOffTimer(); // backoff event �߻��� �۵��ϴ� �Լ�
  void difsTimer(); // difs �߻��� �۵��ϴ� �Լ�
  void sendTimer();// rts->cts, cts->data, data->ack �����͸� ������ �޴µ� �ɸ��� Ÿ�̸� �Լ� �߻��� �۵��ϴ� �Լ�
  void navTimer(); // nav �߻��� �۵��ϴ� �Լ�
  void sifsTimer(); // sifs �߻��� �۵��ϴ� �Լ�
  void eifsTimer(); // eifs �߻��� �۵��ϴ� �Լ�
  bool isIDLE(); // ���� ���°� idle���� true -> idle

  uint32_t m_RetryLimit; // ������ �ִ�Ƚ�� �� 3
  uint32_t m_retryCnt ; // ���� ������ Ƚ��
  uint32_t m_successCount ; //��Ŷ ���� ���� Ƚ��
  uint32_t m_failCount ; // ��Ŷ ���� ���� Ƚ��
  uint32_t APNode ; // AP�� ����� ��ȣ
  State m_prevState; // ������ ���� �������Ⱦ�
  State m_state; // ���� ����
  SendType m_sendType; // ������ ��Ŷ�� Ÿ��

  const int m_nodeMAX; // 255
  uint32_t m_boundary; // �ùķ��̼��� ������ ũ�� �� 1500
  uint32_t m_nodeIncrease; // ù ���� ����ȣ�� ���
  uint32_t m_currNodes; // ������ ����� �� ����
  bool m_cleared; 

  /// Provides uniform random variables.
  Ptr<UniformRandomVariable> m_rv;

  void PhyRxPacketGood (Ptr<Packet> packet, double sinr, UanTxMode mode);
  void PhyRxPacketError (Ptr<Packet> packet, double sinr);
  void EndTx (void);

  void processingSuccess();
  void setCtsPacket();
  void setPoint(); // ��� ����� ��ǥ�� ������ ����, �Ÿ��� ����ϰ� ������
  double CalculateDistance (const Vector2D &a, const Vector2D &b); // �Ÿ�����
  uint32_t splitPoint(Vector2D arr[], uint32_t count); // ��ǥ�� ���ø��ϱ�����
  void sortDistance(); // ����
  uint32_t getSection(Vector2D arr[], int currNode, int APNode); // ������ ����

  uint32_t getNearestNode();
  uint32_t findNearestNode(uint32_t section);
  uint32_t getRandomNode();
  uint32_t findRandomNode(uint32_t section);
  uint32_t getFairnessNode();
  uint32_t findFairnessNode(uint32_t section);

  Time CalculateTime(uint32_t selectedNode);
  void addExcetionCount(uint32_t section);
protected:
  virtual void DoDispose ();
};

}

#endif /* UAN_MAC_CW_H */
