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


#define DEBUG_PRINT  2 //Lv0 : 디버그 메세지 출력 안함	Lv1 : 디버그 성공 횟수 출력 Lv2 : 디버그 메세지 부분 출력		Lv3 : 디버그 메세지 전체 출력
#define TRANSMODE 1 // Lv0 : 일반		Lv1 : 랜덤		 Lv2 : 최적		 Lv3 : 공평성+백오프증가

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
  	/*우리가 추가한거*/
  typedef enum {
    IDLE, CCABUSY, RUNNING, TX, RX, RTSSENT, DATATX, BACKOFF, NAV, EIFS, DIFS
  } State;
  /*우리가 추가한거*/
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
  double m_sifs; // sifs의 시간 0.02초
  double m_difs; // difs의 시간 slotTime*2+sifs
  uint32_t m_ACK; //ACK패킷의 크기 31
  uint32_t m_RTS; //RTS패킷의 크기 37
  uint32_t m_CTS; //CTS패킷의 크기 31
  uint32_t m_DATA; //DATA패킷의 크기 247
  // State variables
  Time m_sendTime; // SendTimer의 시간을 저장하는 변수
  Time m_navTime; // NavTimer의 시간을 저장하는 변수
  Time m_backoffTime; // BackoffTimer의 시간을 저장하는 변수
  Time m_difsTime; // DifsTimer의 시간을 저장하는 변수
  Time m_sifsTime ; // sifsTimer의 시간을 저장하는 변수
  Time m_eifsTime ; // eifsTimer의 시간을 저장하는 변수
  Time m_backoff ; // 백오프 중 중단되어 남은 backoff시간
  Time m_sleepTime; // 데이터를 전송하고 남은 NAV시간 동기화용
  Time m_SumDelays; // maxProp를 설정하였기에 다 보내고 잠들어 있는 시간
  list<Time> m_startPacket; // Delay 계산용 DIFS가 발동된 시간을 기록해둬서 success

  uint32_t m_reset_cw; // 3번의 실패하고 다시 초기화되는 cw의 값 
  uint32_t dataBytes ; //순수 Data 크기 

  bool m_pointFlag; // 처음 모든 좌표를 한번만 받아오기 위한 flag
  bool m_waitFlag; // 연속적인 데이터를 받아 들이기 위한 flag
  std::string m_point; // 좌표를 저장하는 변수
  std::map<uint32_t,pair<double,uint32_t>> m_distance; // Key 노드번호 Value first -> distance second -> section
  vector<pair<uint32_t,pair<double,uint32_t>>> m_sortedDistance; // 거리순으로 정렬되어 있는 vector
  double m_exceptCount; // fairness 가중치

  std::list<std::pair <Ptr<Packet>, UanAddress >> m_pktTx; // 패킷저장용 list
  std::list<uint16_t> m_pktTxProt; // port저장용 list
  uint32_t m_queueLimit; //큐의 제한 현재 10개
  std::map<UanAddress, Time> m_propDelay; // Key 노드번호 value 지연시간
  vector<uint32_t> m_selectedNodes;
  Ptr<Packet> m_transPktTx; //전송할때 사용
  uint16_t m_transPktTxProt;  //전송할때 사용
  Ptr<Packet> m_savePktTx;

  EventId m_sendEvent; //
  EventId m_txEndEvent;
  EventId m_backOffEvent;
  EventId m_difsEvent;
  EventId m_timerEvent;
  EventId m_navEndEvent;
  EventId m_sifsEvent;
  EventId m_eifsEvent;
  
  uint32_t m_currentRate; // datarate 현 1500
  double m_maxPropDelay ; //최대 지연시간 
  double m_mifs ; //0.001
  void backOffTimer(); // backoff event 발생시 작동하는 함수
  void difsTimer(); // difs 발생시 작동하는 함수
  void sendTimer();// rts->cts, cts->data, data->ack 데이터를 보내고 받는데 걸리는 타이머 함수 발생시 작동하는 함수
  void navTimer(); // nav 발생시 작동하는 함수
  void sifsTimer(); // sifs 발생시 작동하는 함수
  void eifsTimer(); // eifs 발생시 작동하는 함수
  bool isIDLE(); // 현재 상태가 idle인지 true -> idle

  uint32_t m_RetryLimit; // 재전송 최대횟수 현 3
  uint32_t m_retryCnt ; // 현재 재전송 횟수
  uint32_t m_successCount ; //패킷 전송 성공 횟수
  uint32_t m_failCount ; // 패킷 전송 실패 횟수
  uint32_t APNode ; // AP의 노드의 번호
  State m_prevState; // 이전의 상태 지금은안씀
  State m_state; // 현재 상태
  SendType m_sendType; // 보내는 패킷의 타입

  const int m_nodeMAX; // 255
  uint32_t m_boundary; // 시뮬레이션의 공간의 크기 현 1500
  uint32_t m_nodeIncrease; // 첫 시작 노드번호를 기록
  uint32_t m_currNodes; // 현재의 노드의 총 갯수
  bool m_cleared; 

  /// Provides uniform random variables.
  Ptr<UniformRandomVariable> m_rv;

  void PhyRxPacketGood (Ptr<Packet> packet, double sinr, UanTxMode mode);
  void PhyRxPacketError (Ptr<Packet> packet, double sinr);
  void EndTx (void);

  void processingSuccess();
  void setCtsPacket();
  void setPoint(); // 모든 노드의 좌표를 가져와 구간, 거리를 계산하고 정렬함
  double CalculateDistance (const Vector2D &a, const Vector2D &b); // 거리계산용
  uint32_t splitPoint(Vector2D arr[], uint32_t count); // 좌표를 스플릿하기위해
  void sortDistance(); // 정렬
  uint32_t getSection(Vector2D arr[], int currNode, int APNode); // 구간을 구함

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
