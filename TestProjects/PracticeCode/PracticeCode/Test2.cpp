//#include <cstdio>
//#include <cstdlib>
//#include <ctime>
//#include <cstring>
//#include "CSerializationBuffer.h"
//
//#pragma pack(push, 1)
//struct PacketHeader
//{
//    BYTE fixedCode;   // 0xBB
//    WORD payloadLen;  // 페이로드 길이
//    BYTE randomKey;   // 1~255 랜덤 키
//    BYTE checksum;    // 페이로드 기준 체크섬
//};
//#pragma pack(pop)
//
//static void DumpBytes(const char* title, const unsigned char* buf, int len)
//{
//    printf("%s :", title);
//    for (int i = 0; i < len; ++i)
//        printf(" %02X", buf[i]);
//    printf("\n");
//}
//
//static BYTE MakeRandomKey()
//{
//    return static_cast<BYTE>((rand() % 0xFE) + 1); // 1~255
//}
//
//void TestPacketEncodeDecode(const char* message, int repeatCount)
//{
//    constexpr BYTE kFixedCode = 0xBB;
//    constexpr BYTE kSessionKey = 0xA9;
//    constexpr int headerSize = sizeof(PacketHeader);
//    constexpr int bufferSize = 1024;
//
//    const int payloadLen = static_cast<int>(strlen(message));
//    const int totalLen = headerSize + payloadLen;
//
//    srand(static_cast<unsigned int>(time(nullptr)));
//
//    for (int iter = 1; iter <= repeatCount; ++iter)
//    {
//        CPacket packet;
//        unsigned char randKey = 0x31;
//        packet.Initialize(bufferSize, headerSize);
//        packet.PutData(const_cast<char*>(message), payloadLen);
//
//        auto* header = reinterpret_cast<PacketHeader*>(packet.GetBufferPtr());
//        header->fixedCode = 0xa9;
//        header->payloadLen = static_cast<WORD>(payloadLen);
//        header->randomKey = rand() % 256;
//
//        DumpBytes("[BEFORE]", reinterpret_cast<unsigned char*>(packet.GetBufferPtr()), totalLen);
//
//        packet.SetCheckSum();
//        DumpBytes("[CHECKSUM]", reinterpret_cast<unsigned char*>(packet.GetBufferPtr()), totalLen);
//
//        packet.Encode(kSessionKey, randKey); // Encode가 내부에서 체크섬을 설정
//        DumpBytes("[ENCODED]", reinterpret_cast<unsigned char*>(packet.GetBufferPtr()), totalLen);
//
//        const bool decodeOk = packet.Decode(kSessionKey, randKey);
//        DumpBytes("[DECODED]", reinterpret_cast<unsigned char*>(packet.GetBufferPtr()), totalLen);
//
//        const unsigned char storedChecksum = header->checksum;      // 복호화 후 헤더에서 읽음
//        const unsigned char calcChecksum = packet.GetCheckSum();    // 복호화된 페이로드 기준 재계산
//        const bool checksumOk = decodeOk && (storedChecksum == calcChecksum);
//
//        char decoded[bufferSize] = {};
//        memcpy(decoded, packet.GetPayloadPtr(), payloadLen);
//        decoded[payloadLen] = '\0';
//
//        printf("[%d] Payload  : %s\n\n", iter, decoded);
//        printf("[%d] Checksum : stored=%02X / calc=%02X (%s)\n\n",
//            iter, storedChecksum, calcChecksum, checksumOk ? "OK" : "FAIL");
//    }
//}
//
//int main()
//{
//    TestPacketEncodeDecode("aaaaaaaaaabbbbbbbbbbcccccccccc1234567890abcdefghijklmn", 5);
//    return 0;
//}