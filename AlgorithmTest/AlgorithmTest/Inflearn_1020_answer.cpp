//#include <iostream>
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
//int dir[4][2] = { {0,1}, {0, -1}, { 1, 0 }, {-1, 0} };
//
//int forest[1001][1001];
//int dist[2][1001][1001];
//
//queue<Coord>Q;
//int W, H, answer;
//
//void BFS(int idx)
//{
//    while (!Q.empty())
//    {
//        int x = Q.front().x;
//        int y = Q.front().y;
//        Q.pop();
//        for (int i = 0; i < 4; i++)
//        {
//            int nx = x + dir[i][0];
//            int ny = y + dir[i][1];
//
//            if (nx < 0 || nx >= W || ny < 0 || ny >= H)
//                continue;
//
//            if (forest[ny][nx] == 1)
//                continue;
//
//            if (dist[idx][ny][nx] != 0)
//                continue;
//
//            dist[idx][ny][nx] = dist[idx][y][x] + 1;
//            Q.push({ nx, ny });
//        }
//    }
//}
//
//void Solution(Coord& startPoint, Coord& knightPoint)
//{
//    dist[0][startPoint.y][startPoint.x] = 1;
//    Q.push(startPoint);
//    BFS(0);
//
//    dist[1][knightPoint.y][knightPoint.x] = 1;
//    Q.push(knightPoint);
//    BFS(1);
//
//    int result = 2147483647;
//    for (int i = 0; i < H; i++)
//    {
//        for (int j = 0; j < W; j++)
//        {
//            if (forest[j][i] == 4 && dist[0][j][i] > 0 && dist[1][j][i] > 0)
//            {
//                int res = dist[0][j][i] + dist[1][j][i];
//                answer = min(res, answer);
//            }
//        }
//    }
//}
//
//int main() {
//    ios_base::sync_with_stdio(false);
//
//    Coord startPoint;
//    Coord knightPoint;
//
//    cin >> W >> H;
//    for (int i = 0; i < H; i++)
//    {
//        for (int j = 0; j < W; j++)
//        {
//            cin >> forest[i][j];
//
//            if (forest[i][j] == 2)
//            {
//                startPoint.x = j;
//                startPoint.y = i;
//            }
//
//            if (forest[i][j] == 3)
//            {
//                knightPoint.x = j;
//                knightPoint.y = i;
//            }
//        }
//    }
//    cin.ignore();
//
//    answer = 2147483647;
//
//    Solution(startPoint, knightPoint);
//
//    cout << answer - 2;
//
//    return 0;
//}