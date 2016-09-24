#include <iostream>
#include <vector>
#include <unordered_map>
#include <random>
#include <unordered_set>
#include <math.h>
#include <chrono>

using namespace std;

// Maximum allowed duration
chrono::milliseconds max_duration = chrono::milliseconds(95);

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


vector<vector<Point*>> points;
int width = 30, height = 20;

class Board{
    unordered_set<Point*> blocked;

    public:
    Board(){}

    Board(unordered_set<Point*> _blocked){
        blocked = _blocked;
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

        if(top != nullptr){
            neighbors.push_back(top);
        }

        if(bottom != nullptr){
            neighbors.push_back(bottom);
        }

        if(left != nullptr){
            neighbors.push_back(left);
        }

        if(right != nullptr){
            neighbors.push_back(right);
        }

        return neighbors;
    }

    vector<Point*> validNeighbors(int x, int y){
        vector<Point*> valid;
        for(Point* p : getNeighbors(x, y)){
            if( !isBlocked(p) ){
                valid.push_back(p);
            }
        }

        return valid;
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
        vector<Point*> possibleMoves = board.validNeighbors(position->getX(), position->getY());

        if( !possibleMoves.empty()  ){
            return possibleMoves.at((unsigned int) randomIntRange(0, possibleMoves.size() - 1));
        }else{
            // If the enemy cant return a valid choice it has lost
            return nullptr;
        }
    }



};


double biasParameter = sqrt(2);

class ActionTree {
    public:
    Point* move;
    vector<ActionTree*> children;
    ActionTree* parent = nullptr;
    int wins = 0, plays = 0;
    bool winningMove;

    ActionTree(Point* _move, ActionTree* _parent){
        move = _move;
        wins = 0;
        plays = 0;
        winningMove = false;
        parent = _parent;
    }

    void win(){
        wins += 1;
        plays += 1;

        if( parent != nullptr){
            parent->win();
        }
    }

    void loss(){
        plays += 1;

        if( parent != nullptr ){
            parent->loss();
        }
    }

    double ubcScore(){
        double winPercentage = 0, exploration = 0;
        // Avoid division by zero

        if (plays != 0){
            winPercentage = wins / plays;

            if( parent != nullptr ){
                exploration = biasParameter * sqrt( (log(parent->plays / plays)) );
            }
        }
        return winPercentage + exploration;
    }


    /* Get the best child based ubc score: higher is better */
    ActionTree* getOptimalChild(Board& board){
        ActionTree* bestChild = nullptr;
        double bestScore = -INFINITY;

        for(ActionTree* child : children){
            if( !board.isBlocked(child->move) ){
                double ubcScore = child->ubcScore();
                if( bestScore < ubcScore){
                    bestScore = ubcScore;
                    bestChild = child;
                }
            }
        }

        return bestChild;
    }

    ActionTree* getWinRatioChild(){
        ActionTree* bestChild = nullptr;
        double bestScore = -INFINITY;

        for(ActionTree* child : children){
            if( child->plays > 0 ){
                double score = child->wins / child->plays;
                if( bestScore < score){
                    bestScore = score;
                    bestChild = child;
                }
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
    MCTS(){

    };

    Point* getBestChoice(Board board, vector<Player> enemies, Player player){
        rootNode = new ActionTree(player.position, nullptr);


        auto begin = std::chrono::steady_clock::now();
        int timediff =  int(chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count());



        //RESETLOOP:while ( int(chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count()) < 95 ) {
        int count = 0;
        RESETLOOP:while ( count++ < 1500) {
            // Start from the rootnode
            ActionTree* currentNode = rootNode;
            Board currentBoard = Board(board);
            vector<Player> currentEnemies;
            Player currentPlayer;

            while( true ){
                for(Player e: currentEnemies){
                    Point* newPosition = e.getRandomMove(board);
                    e.position = newPosition;

                    // They have lost
                    if( newPosition == nullptr){
                        currentNode->win();
                        goto RESETLOOP;
                    }else{
                        currentBoard.setBlocked(newPosition);
                    }
                }

                // The node has not been visited before, get neighbors as children
                if( currentNode->plays == 0 ){

                    vector<ActionTree*> children = {};
                    // Loop through and add children
                    for(Point* p: currentBoard.getNeighbors(currentNode->move->getX(), currentNode->move->getY())){
                        children.push_back(new ActionTree(p, currentNode));
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
                    goto RESETLOOP;

                    // The node has been visited before
                }else{
                    ActionTree* bestChild = currentNode->getOptimalChild(currentBoard);
                    // if the best child is nullptr, we have lost
                    if( bestChild == nullptr ){
                        currentNode->loss();
                        goto RESETLOOP;
                    }else{
                        currentNode = bestChild;
                        currentBoard.setBlocked(currentNode->move);
                    }
                }
            }
        }

        ActionTree* bestChild = rootNode->getWinRatioChild();

        // We cant do anything, do something just to end the turn
        if ( bestChild == nullptr){
            return board.getPoint(0, 0);
        }else{
            return bestChild->move;
        }

    };



};



int main() {
    for(int x = 0; x < width; x++){
        vector<Point*> yVector;
        for(int y = 0; y < height; y++){
            Point* p = new Point(x, y);
            yVector.push_back(p);
        }
        points.push_back(yVector);
    }


    MCTS mcts = MCTS();
    Board board = Board();

    Player enemy =  Player(board.getPoint(10, 10));
    vector<Player> enemies = { enemy };
    Player player = Player(board.getPoint(15, 15));

    board.setBlocked(player.position);
    board.setBlocked(enemy.position);




    Point* result = mcts.getBestChoice(board, enemies, player);
    cout << "The best move is: (" << result->getX() << ", " << result->getY() << ")" << std::endl;

    return 0;
}