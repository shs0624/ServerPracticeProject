//#include <iostream>
//#include <vector>
//#include <queue>
//#include <algorithm>
//using namespace std;
//
//struct Coord
//{
//    int x;
//    int y;
//};
//
//int n;
//int room[101][101];
//int check[101][101];
//Coord tom;
//Coord jerry;
//
//Coord dir[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
//
//void Solution()
//{
//    if (tom.x == jerry.x && tom.y == jerry.y)
//    {
//        cout << 0 << "\n";
//        return;
//    }
//
//    queue<Coord> q;
//
//    q.push(tom);
//    check[tom.x][tom.y] = -1;
//
//    while (!q.empty())
//    {
//        int x = q.front().x;
//        int y = q.front().y;
//        q.pop();
//
//        for (int i = 0; i < 4; i++)
//        {
//            int nx = x;
//            int ny = y;
//            while (1)
//            {
//                nx += dir[i].x;
//                ny += dir[i].y;
//
//                if (nx <= 0 || nx > n || ny <= 0 || ny > n)
//                    break;
//
//                if (room[nx][ny] == 1)
//                    break;
//
//                if ((check[nx][ny] == -1) || (check[nx][ny] > check[x][y] + 1))
//                {
//                    check[nx][ny] = check[x][y] + 1;
//                    q.push({ nx,ny });
//                }
//            }
//        }
//    }
//
//    cout << check[jerry.x][jerry.y];
//}
//
//int main()                                               
//{
//    ios_base::sync_with_stdio(false);
//
//    cin >> n;
//    for (int i = 1; i <= n; i++)
//    {
//        for (int j = 1; j <= n; j++)
//        {
//            cin >> room[i][j];
//            check[i][j] = -1;
//        }
//    }
//    cin >> tom.x >> tom.y;
//    cin >> jerry.x >> jerry.y;
//
//    Solution();
//
//    return 0;
//}