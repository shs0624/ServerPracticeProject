#include <iostream>
#include <queue>
#include <algorithm>
using namespace std;

struct Coord
{
    int x;
    int y;
};

int W, H, answer;
int forest[1001][1001];
char check[1001][1001];

char knightCheck[1001][1001] = { 0 };
int dist[1001][1001] = { 0 };

int dir[4][2] = { {0,1}, {0, -1}, { 1, 0 }, {-1, 0} };

void Solution(Coord& startCoord, Coord& knightCoord)
{
    int bStrawberry = 0;
    queue<pair<Coord, int>> Q;
    queue<pair<Coord, int>> berryQ;

    Q.push({ startCoord, 0 });
    check[startCoord.y][startCoord.x] = 1;

    while (!Q.empty())
    {
        Coord c = Q.front().first;
        int result = Q.front().second;

        Q.pop();

        if (forest[c.y][c.x] == 4)
        {
            startCoord.x = c.x;
            startCoord.y = c.y;

            berryQ.push({ { c.x, c.y }, result });
            continue;
        }
       
        for (int i = 0; i < 4; i++)
        {
            int nx = c.x + dir[i][0];
            int ny = c.y + dir[i][1];

            if (nx < 0 || nx >= W || ny < 0 || ny >= H)
                continue;

            if (forest[ny][nx] == 1 || forest[ny][nx] == 3)
                continue;

            if (check[ny][nx] == 1)
                continue;

            check[ny][nx] = 1;
            Q.push({ {nx,ny},result + 1 });
        }
    }

    queue<pair<Coord, int>> knightFindQ;
    knightFindQ.push({ knightCoord, 0 });

    while (!knightFindQ.empty())
    {
        Coord knightP = knightFindQ.front().first;
        int distResult = knightFindQ.front().second;

        knightFindQ.pop();
        
        if (forest[knightP.y][knightP.x] == 4)
        {
            continue;
        }

        for (int i = 0; i < 4; i++)
        {
            int nx = knightP.x + dir[i][0];
            int ny = knightP.y + dir[i][1];

            if (nx < 0 || nx >= W || ny < 0 || ny >= H)
                continue;

            if (forest[ny][nx] == 1)
                continue;

            if (knightCheck[ny][nx] == 1)
                continue;

            knightCheck[ny][nx] = 1;
            dist[ny][nx] = distResult + 1;
            knightFindQ.push({ {nx,ny},distResult + 1 });
        }
    }

    while (!berryQ.empty())
    {
        int x = berryQ.front().first.x;
        int y = berryQ.front().first.y;

        int berryDist = berryQ.front().second;

        berryQ.pop();

        answer = min(dist[y][x] + berryDist, answer);
    }
}

int main() {
    ios_base::sync_with_stdio(false);
    Coord startPoint;
    Coord knightPoint;

    cin >> W >> H;
    for (int i = 0; i < H; i++)
    {
        for (int j = 0; j < W; j++)
        {
            cin >> forest[i][j];

            if (forest[i][j] == 2)
            {
                startPoint.x = j;
                startPoint.y = i;
            }

            if (forest[i][j] == 3)
            {
                knightPoint.x = j;
                knightPoint.y = i;
            }
        }
    }
    cin.ignore();

    answer = 2147483647;

    Solution(startPoint, knightPoint);

    cout << answer;

    return 0;
}