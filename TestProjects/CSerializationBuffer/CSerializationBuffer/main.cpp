#include "CSerializationBuffer.h"
#include <iostream>

int main()
{
	int ival;
	float fval;
	double dval;
	long long llval;
	CPacket* cpacket = new CPacket(100);
	CPacket* cpacket2 = new CPacket(100);

	*cpacket << (float)3.14;
	*cpacket >> fval;
	printf("%f\n", fval);
	
	*cpacket << (int)3;
	*cpacket >> ival;
	printf("%d\n", ival);

	*cpacket << (double)20.55;
	*cpacket >> dval;
	printf("%f\n", dval);

	*cpacket << (long long)3096;
	*cpacket >> llval;
	printf("%d\n", llval);

	char buf[13];
	strcpy_s(buf, 13, "abcdefghijkl");
	cpacket->PutData(buf, 13);

	cpacket2 = cpacket;

	char temp[13];
	cpacket->GetData(temp, 13);
	printf("%s\n", temp);

	cpacket2->GetData(temp, 13);
	printf("%s\n", temp);

	Sleep(400);

	return 0;
}