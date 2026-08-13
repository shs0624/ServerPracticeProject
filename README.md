# [서버 개발자] 성현식

## [목차]

- [IOCP기반 컨텐츠별 스레드 분리 게임 서버](#IOCP-기반-컨텐츠별-스레드-분리-게임-서버)
- [IOCP기반 멀티스레드 채팅 서버](#IOCP-기반-멀티스레드-채팅-서버)
- [MMOTCP Select기반 게임 서버](#MMOTCP-Select-기반-게임-서버)

## IOCP 기반 컨텐츠별 스레드 분리 게임서버

**`[프로젝트 개요]`**

IOCP 기반 네트워크 라이브러리를 활용해 컨텐츠 별로 별도의 스레드로 분리해 구현한 게임 서버입니다.

이 서버엔 인증 스레드와 에코 스레드를 분리하여 구현했습니다.

개발은 혼자 진행했고, 5,000개의 더미 클라이언트로 스트레스 테스트를 진행했습니다.

**`[기간]`**

2026.01.30 ~ 2026.02.27 (1개월)

**`[스레드구조]`**
<details>
<summary>스레드구조</summary>
<div markdown="1" style="padding-left: 15px;">
<img width="2000" height="756" alt="image" src="https://github.com/user-attachments/assets/4e630a11-c339-4fb7-a718-a6646891f46f" />
</div>
</details>

**`[개발 내용]`**

- 네트워크 IO는 IOCP 스레드가 맡고, 인증과 게임 서버를 각각 별도 스레드가 매 프레임 소속 세션의 메세지를 처리하도록 구현 
  - [RoomThread 구현](https://github.com/shs0624/ServerPracticeProject/blob/3cc4f45d1c68777c1125a2944ab8311a2ac7e6ba/Portfolio/IOCP_GamePipeServer/IOCP_GamePipeServer/RoomNetServer.cpp#L166-L205)
- 컨텐츠 스레드마다 락프리큐를 두고, 그 큐로 컨텐츠 영역으로의 세션의 입장/퇴장을 관리하는 구조로 구현 
  - [Enter,Leave 구현](https://github.com/shs0624/ServerPracticeProject/blob/3cc4f45d1c68777c1125a2944ab8311a2ac7e6ba/Portfolio/IOCP_GamePipeServer/IOCP_GamePipeServer/IRoom.h#L31-L55)
- 송신 과정에서 락경합을 최소화하기 위해 송신 버퍼에 락프리큐 적용 
  - [락프리큐](https://github.com/shs0624/ServerPracticeProject/blob/3cc4f45d1c68777c1125a2944ab8311a2ac7e6ba/Portfolio/IOCP_GamePipeServer/IOCP_GamePipeServer/LockFreeQueue.h#L52)
- 락프리큐의 노드 할당과 반환도 락경합을 최소화하기 위해 TLS(Thread Local Storage)메모리 풀을 구현하여 적용 
  - [TLS메모리풀](https://github.com/shs0624/ServerPracticeProject/blob/3cc4f45d1c68777c1125a2944ab8311a2ac7e6ba/Portfolio/IOCP_GamePipeServer/IOCP_GamePipeServer/TLSMemoryPool.h#L27)

**`[사용기술]`**

- Language : C++

## IOCP 기반 멀티스레드 채팅서버

**`[프로젝트 개요]`**

구현한 IOCP 기반 네트워크 라이브러리를 활용한 채팅 서버입니다.

섹터링 방식을 적용해 자신 섹터 주변에 채팅을 전송하는 서버입니다.

개발은 혼자 진행했고, 15,000개의 더미 클라이언트로 스트레스 테스트를 진행했습니다.

**`[기간]`**

2025.12.01 ~ 2026.01.21 (1개월)

**`[스레드구조]`**
<details>
<summary>멀티스레드 스레드구조</summary>
<div markdown="1" style="padding-left: 15px;">
<img width="2000" height="738" alt="image" src="https://github.com/user-attachments/assets/0a9d9228-efe3-44bc-a00f-24c37d3d581e" />
</div>
</details>

**`[개발 내용]`**

- 로그인 과정의 DB 통신 부하를 덜기 위해, 로그인 서버를 분리 후 클라이언트가 제시한 세션키를 MySQL로 계정 유효성 검증한 뒤 Redis에 캐싱한 뒤, 채팅 서버가 Redis에서 재인증하여 DB에 접근하지 않도록 구현 
  - [로그인과정](https://github.com/shs0624/ServerPracticeProject/blob/3cc4f45d1c68777c1125a2944ab8311a2ac7e6ba/Portfolio/IOCP_ChatServer_MultiThread/IOCP_ChatServer_MultiThread/ChatServer_Message.cpp#L8)
- 스레드 아키텍처는 IO 완료통지를 처리하는 IOCP 스레드가 별도의 구분 없이 컨텐츠 영역의 작업까지 도맡아 하는 구조로, 이벤트 함수를 가상함수로 제공하고 이를 컨텐츠 서버가 오버라이드 하는 형태로 구현
  - [NetServer 헤더](https://github.com/shs0624/ServerPracticeProject/blob/39464c47358f6a5a4f2d9403a2907bcd1a28bce8/Portfolio/IOCP_ChatServer_MultiThread/IOCP_ChatServer_MultiThread/NetServer.h#L51-L80)
- 섹터링 방식을 적용해 유저는 자기 섹터와 주변 8방향의 유저에게 채팅을 전송하거나, 섹터를 이동하는 동작이 가능하도록 구현

**`[사용기술]`**

- Language : C++
- Database : MySQL, Redis

## MMOTCP Select기반 게임 서버

**`[프로젝트 개요]`**

Select 함수를 이용해서 네트워크 I/O를 처리하는 MMO 게임 서버입니다.

섹터링 방식을 적용해 50x50 개의 섹터로 맵을 구현했고, 캐릭터의 이동과 공격을 구현했습니다.

개발은 혼자 진행했고, 10,000개의 더미 클라이언트로 스트레스 테스트를 진행했습니다.

**`[기간]`**

2025.03.14 ~ 2025.04.24, 2025.06.16 ~ 2025.07.05 (실작업 2개월)

**`[스레드구조]`**
<details>
<summary>Select MMOTCP 게임 서버 스레드 구조</summary>
<div markdown="1" style="padding-left: 15px;">
<img width="2000" height="771" alt="image" src="https://github.com/user-attachments/assets/7bd51610-0207-46e6-8159-56575ed261d0" />
</div>
</details>

**`[개발 내용]`**

- Select기반으로 IO를 진행하며, 실제 접속에 비해 부족한 FD_SETSIZE는 반복을 통해 해결하고, Accept도 매 반복마다 확인할 수 있도록 한 자리는 ListenSocket을 설정했습니다.
  - [Select 처리](https://github.com/shs0624/ServerPracticeProject/blob/c61ccc4043166b1d206aac8174768820b07d3072/Portfolio/TCPFighter_MMO/TCPFighter_MMO/TCPNetwork.cpp#L127-L150)
- 섹터 이동은 이동 후 방향과 이동 전 방향을 비교해서 내 캐릭터가 보이지 않아야 하는 섹터에 캐릭터 삭제 메세지를, 보이게 되어야 하는 섹터에 캐릭터 생성 메세지를 전송합니다. 
  - 섹터 선별은 최적화를 위해 미리 방향에 따라 체크할 섹터 위치를 설정해두었습니다.
  - [섹터 이동](https://github.com/shs0624/ServerPracticeProject/blob/c61ccc4043166b1d206aac8174768820b07d3072/Portfolio/TCPFighter_MMO/TCPFighter_MMO/SectorProc.cpp#L129)
- 이동 과정에서 클라이언트와 서버의 위치 정보가 틀어질 수 있기 때문에, 일정 수치 이상 틀어진다면 클라이언트의 위치를 서버 위치로 정정하는 패킷을 전송하게 했습니다.
  - [위치 동기화](https://github.com/shs0624/ServerPracticeProject/blob/c61ccc4043166b1d206aac8174768820b07d3072/Portfolio/TCPFighter_MMO/TCPFighter_MMO/ContentsProc.cpp#L150-L156)

**`[사용기술]`**

- Language : C++