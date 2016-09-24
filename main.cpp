#include <iostream>
#include <vector>
#include <unordered_map>
#include <random>
#include <unordered_set>
#include <math.h>
#include <chrono>

using namespace std;



int randomIntRange(int min, int max){
    random_device rseed;
    mt19937 rgen(rseed()); // mersenne_twister
    uniform_int_distribution<int> idist(min, max);
    return idist(rgen);
}

class Point {
    int x, y;

    public:
    Point(int _x, int _y){
        x = _x;
        y = _y;
    }

    Point(){
        x = 0;
        y = 0;
    }

    const int getX() const {
        return x;
    }

    const int getY() const {
        return y;
    }

    bool operator==(const Point &other) const {
        return (x == other.x && y == other.y);
    }
};


namespace std
{
    template <>
    struct hash<Point>
    {
        size_t operator()(const Point& k) const
        {
            // Compute individual hash values for two data members and combine them using XOR and bit shifting
            return ((hash<int>()(k.getX()) ^ (hash<int>()(k.getY()) << 1)) >> 1);
        }
    };
}


class Board{
    static vector<vector<Point*>> points;
    static int width, height;
    unordered_set<Point*> blocked;

    public:
    Board(int _width, int _height){
        width = _width;
        height = _height;

        for(int x = 0; x < width; x++){
            vector<Point*> yVector;
            for(int y = 0; y < height; y++){
                Point* p = new Point(x, y);
                yVector.push_back(p);
            }
            points.push_back(yVector);
        }
    }

    void setBlocked(Point* p){
        blocked.insert(p);
    }

    bool isBlocked(Point* p){
        return blocked.find(p) != blocked.end();
    }

    Point* getPoint(int x, int y){
        if(x >= 0 && width > x && y >= 0 && height > y){
            return points.at(x).at(y);
        }else{
            return nullptr;
        }
    }

    vector<Point*> getNeighbors(int x, int y){
        vector<Point*> neighbors;

        Point* bottom = getPoint(x, y-1);
        Point* top = getPoint(x, y+1);
        Point* left = getPoint(x-1, y);
        Point* right = getPoint(x+1, y);

        if(top != nullptr && !isBlocked(top)){
            neighbors.push_back(top);
        }

        if(bottom != nullptr && !isBlocked(bottom)){
            neighbors.push_back(bottom);
        }

        if(left != nullptr && !isBlocked(left)){
            neighbors.push_back(left);
        }

        if(right != nullptr && !isBlocked(right)){
            neighbors.push_back(right);
        }

        return neighbors;
    }
};


class Player{
    public:
    Point* position;
    Player(Point* pos){
        position = pos;
    }

    Player(){}

    Point* getRandomMove(Board& board){
        vector<Point*> possibleMoves = board.getNeighbors(position->getX(), position->getY());
        if( !possibleMoves.empty()  ){
            return possibleMoves.at((unsigned int) randomIntRange(0, possibleMoves.size() - 1));
        }else{
            // If the enemy cant return a valid choice it has lost
            return nullptr;
        }
    }



};


class ActionTree {
    public:
    static double biasParameter;
    Point* move;
    vector<ActionTree*> children;
    ActionTree* parent;
    int wins, plays;
    bool winningMove;

    ActionTree(Point* _move){
        move = _move;
    }

    void win(){
        wins += 1;
        plays += 1;

        if( parent != nullptr){
            parent->win();
        }
    }

    bool isLeaf(){
        return plays > 0 && children.empty();
    }

    void loss(){
        plays += 1;

        if( parent != nullptr ){
            parent->loss();
        }
    }

    double ubcScore(){
        double winPercentage = wins / plays;
        double exploration = 0;
        if( parent != nullptr ){
            exploration = biasParameter * sqrt( (log(parent->plays / plays)) );
        }

        return winPercentage + exploration;
    }


    /* Get the best child based ubc score: higher is better */
    ActionTree* getOptimalChild(){
        ActionTree* bestChild = nullptr;
        double bestScore = 0;

        for(ActionTree* child : children){
            double ubcScore = child->ubcScore();
            if( bestScore < ubcScore){
                bestScore = ubcScore;
                bestChild = child;
            }
        }

        return bestChild;
    }

    ActionTree* getWinRatioChild(){
        ActionTree* bestChild = nullptr;
        double bestScore = 0;

        for(ActionTree* child : children){
            double score = child->wins / child->plays;
            if( bestScore < score){
                bestScore = score;
                bestChild = child;
            }
        }

        return bestChild;
    }




};

class MCTS {
    ActionTree* rootNode;

    bool playSimulation(Board board, vector<Player> enemies, Player player){
        // Run until we are the only one standing
        while( !enemies.empty() ){
            // The enemies makes the first move
            for(Player enemy : enemies){
                // Make a random move
                Point* move = enemy.getRandomMove(board);

                // Check if the move is a valid one i.e. not nullpointer
                if ( move == nullptr ){
                    // Needs to be adjused for more players
                    return true;
                }

                // Move the player to the new positon
                enemy.position = move;

                // And mark the old one as used
                board.setBlocked(move);
            }

            Point* playerMove = player.getRandomMove(board);
            if( playerMove == nullptr ){
                return false;
            }
            player.position = playerMove;
            board.setBlocked(playerMove);
        }
    }

    public:
    // Maximum allowed duration
    static chrono::milliseconds max_duration;

    MCTS(){

    };

    Point* getBestChoice(Board board, vector<Player> enemies, Player player){
        rootNode = new ActionTree(player.position);


        // Init the clock
        chrono::time_point<std::chrono::system_clock> start;
        start = chrono::system_clock::now();


        while ( (chrono::system_clock::now()-start) < max_duration ) {
            // Start from the rootnode
            ActionTree* currentNode = rootNode;
            Board currentBoard = Board(1, 1);
            vector<Player> currentEnemies;
            Player currentPlayer;

            while( !currentNode->isLeaf() ){

                // The node has not been visited before, get neighbors as children
                if( currentNode->plays == 0 ){

                    vector<ActionTree*> children = {};
                    // Loop through and add children
                    for(Point* p: currentBoard.getNeighbors(currentNode->move->getX(), currentNode->move->getY())){
                        children.push_back(new ActionTree(p));
                    }

                    // Add the children to the node
                    currentNode->children = children;

                    // First time visiting node, play a simulation
                    if( playSimulation(currentBoard, currentEnemies, currentPlayer) ){
                        currentNode->win();
                    }else{
                        currentNode->loss();
                    }

                    // Start again from rootnode
                    break;

                    // The node has been visited before
                }else{
                    currentNode = currentNode->getOptimalChild();

                }
            }

            if ( currentNode->isLeaf() ){
                // Is the current node a winning leaf?
                if( currentNode->winningMove ){
                    currentNode->win();
                    // The current move is a loosing move
                }else{
                    currentNode->loss();
                }
            }

        }

        ActionTree* bestChild = rootNode->getWinRatioChild();

        return bestChild->move;

    };



};



int main() {
    ActionTree::biasParameter = sqrt(2);
    MCTS::max_duration = chrono::milliseconds(98);
    MCTS mcts = MCTS();
    Board board = Board(30, 20);

    Player enemy =  Player(board.getPoint(10, 10));
    vector<Player> enemies = { enemy };
    Player player = Player(board.getPoint(15, 15));

    board.setBlocked(player.position);
    board.setBlocked(enemy.position);


    mcts.getBestChoice(board, enemies, player);



    std::cout << "Hello, World! 2" << std::endl;
    return 0;
}