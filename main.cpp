#include <iostream>
#include <vector>
#include <unordered_map>
#include <random>
#include <unordered_set>
#include <math.h>
#include <chrono>
#include <string>

using namespace std;

// Maximum allowed duration
int maxDuration = 98;
int heuristicConstant = 1;
double biasParameter = 1 / sqrt(2);
bool TESTING = true;

random_device rseed;
mt19937 rgen(rseed()); // mersenne_twister


int randomIntRange(int min, int max){
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

    Board(const Board& board) : blocked(board.blocked){}

    void setBlocked(Point* p){
        blocked.insert(p);
    }

    unordered_set<Point*> getBlocked(){
        return blocked;
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
        for(Point*& p : getNeighbors(x, y)){
            if( !isBlocked(p) ){
                valid.push_back(p);
            }
        }

        return valid;
    }

    operator std::string() const {
        return "Hi";
    }

};


class Player{
    public:
    Point* position;

    explicit Player(Point* pos) : position(pos){}

    Player(const Player& p){
        position = p.position;
    }

    Player(){}

    Point* getRandomMove(Board& board){
        vector<Point*> possibleMoves = board.validNeighbors(position->getX(), position->getY());

        if( !possibleMoves.empty()  ){
            int randomPos = (unsigned int) randomIntRange(1, possibleMoves.size()) - 1;
            return possibleMoves.at(randomPos);
        }else{
            // If the enemy cant return a valid choice it has lost
            return nullptr;
        }
    }

    void setPosition(Point* p){
        this->position = p;
    }

    Point* getPosition(){
        return this->position;
    }

    Point* getSemiRandomMove(Board& board, vector<Player>& enemies){
        vector<Point*> possibleMoves = board.validNeighbors(position->getX(), position->getY());

        Point* bestMove = nullptr;
        double bestScore = numeric_limits<double>::max();

        for(Point* move: possibleMoves){
            double currentScore = domainHeuristic(enemies, move);

            if( bestScore > currentScore ){
                bestScore = currentScore;
                bestMove = move;
            }
        }


        if( !possibleMoves.empty()  ){
            if( randomIntRange(1, 100) > 60 ){
                return bestMove;
            }else{
                int randomPos = (unsigned int) randomIntRange(1, possibleMoves.size()) - 1;
                return possibleMoves.at(randomPos);
            }
        }else{
            // If the enemy cant return a valid choice it has lost
            return nullptr;
        }
    }

    /**
    * Using a domain specific heuristic that should help the algorithm choose a better path
    * This will primarily be used for simulating moves for the evemies so that they dont loose as easily in the simulations
    * @param enemies
    * @param player
    * @return
    */

    double domainHeuristic(vector<Player>& enemies, Point*& playerPosition){
        double heuristic = 0;

        for(Player e: enemies){
            heuristic += abs(e.position->getX() - playerPosition->getX()) + abs(e.position->getY() - playerPosition->getY());
        }

        return heuristic;
    }
};



class ActionTree {
    public:
    Point* move;
    vector<ActionTree*> children;
    ActionTree* parent = nullptr;
    float wins = 0, plays = 0;

    ActionTree(Point* _move, ActionTree* _parent){
        move = _move;
        wins = 0;
        plays = 0;
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

    void deleteTree(){
        for(ActionTree*& child : children){
            child->deleteTree();
        }
        delete this;
    }

    double getVariance(){
        int totalWins = 0;
        int total = 0;
        int rewardSquared = 0;

        if( parent != nullptr){
            for(ActionTree*& child : parent->children){
                total += child->plays;
                totalWins += child->wins;

                rewardSquared += pow(child->wins/child->plays, 2);
            }
        }

        if( total > 0 ){
            return rewardSquared - pow(totalWins/total, 2);
        }else {
            return 0;
        }
    }

    double ubc1TunedScore(){
        double winPercentage = 0, exploration = 0;
        // Avoid division by zero

        if (plays != 0){
            winPercentage = wins / plays;

            if( parent != nullptr ){
                 exploration = 2 * biasParameter * sqrt( 2*log(parent->plays) / plays );

                //exploration = sqrt(  (log(parent->plays) / plays) *
                //                     min(0.25,
                //                     (getVariance() + sqrt(2 * log(parent->plays) / plays)  )));
            }
        }

        // +        return avg_payoff + math.sqrt(math.log(number_sampled) / sampled_arm.total) * min(MAX_BERNOULLI_RANDOM_VARIABLE_VARIANCE, variance + math.sqrt(2.0 * math.log(number_sampled) / sampled_arm.total))

        return winPercentage + exploration;
    }


    /* Get the best child based ubc score: higher is better */
    ActionTree* getOptimalChild(Board& board){
        ActionTree* bestChild = nullptr;
        double bestScore = -INFINITY;

        for(ActionTree*& child : children){
            if( !board.isBlocked(child->move) ){
                double ubcScore = child->ubc1TunedScore();
                if( bestScore < ubcScore){
                    bestScore = ubcScore;
                    bestChild = child;
                }
            }
        }

        return bestChild;
    }

    ActionTree* getFinalChoiceChild(){
        cerr << "Position: " <<  move->getX() << " " << move->getY() << endl;
        cerr << "Amount children " << children.size() << endl;

        ActionTree* bestChild = nullptr;
        double bestScore = -INFINITY;

        for(ActionTree*& child : children){
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

    bool simulateMove(Point*& move, Board board, vector<Player>& enemies, Player player){
        if( board.isBlocked(move) ){
            return false;
        }else{
            board.setBlocked(move);
            player.position = move;
            return playSimulation(board, enemies, player);
        }
    }

    bool playSimulation(Board& board, vector<Player> enemies, Player player){
        vector<Player> playerVector = {player};


        // Run until we are the only one standing
        while( !enemies.empty() ){
            // The enemies makes the first move
            for(Player& enemy : enemies){
                // Make a random move
                //Point* move = enemy.getRandomMove(board);
                Point* move = enemy.getSemiRandomMove(board, playerVector);

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
        rootNode = nullptr;
    };

    Point* getBestChoice(const Board& board, const vector<Player>& enemies, const Player& player){
        if( rootNode == nullptr){
            rootNode = new ActionTree(player.position, nullptr);
        }

        int count = 0;
        auto begin = std::chrono::steady_clock::now();
        //RESETLOOP:while ( int(chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count()) < maxDuration ) {

            count++;
        RESETLOOP:while ( count++ < 150) {
            // Start from the rootnode
            ActionTree* currentNode = rootNode;
            Board currentBoard = Board(board);
            vector<Player> currentEnemies = vector<Player>{enemies};
            Player currentPlayer = Player(player);

            while( true ){
                for(Player& e: currentEnemies){
                    Point* newPosition = e.getRandomMove(currentBoard);
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
                if( currentNode->plays < 2 ){
                    vector<ActionTree*> children = {};
                    // Loop through and add children
                    for(Point*& p: currentBoard.getNeighbors(currentNode->move->getX(), currentNode->move->getY())){
                        ActionTree* childAction = new ActionTree(p, currentNode);

                        // check if this is a valid move
                        if( simulateMove(p, currentBoard, currentEnemies, currentPlayer) ){
                            childAction->win();
                        }else {
                            childAction->loss();
                        }

                        // We need to add a simulation for each child
                        children.push_back(childAction);
                    }

                    // Add the children to the node
                    currentNode->children = children;

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
                        currentPlayer.position = bestChild->move;
                    }
                }
            }
        }

        cerr << " Amount in while " << count << endl;

        // Continue using the branch that we choose -> more simulations will lead to
        // better result and this will lead to more and more simulations as the game progress.

        rootNode = rootNode->getFinalChoiceChild();
        // We cant do anything, do something just to end the turn
        if ( rootNode == nullptr){
            return new Point(-1, -1);
        }else{
            rootNode->parent = nullptr;
            return rootNode->move;
        }

    };
};

string getDirection(Point* origin, Point* destination){
    if( origin->getX() == destination->getX() ){
        if( origin->getY() > destination->getY() ){
            return "UP";
        }else{
            return "DOWN";
        }
    }else{
        if( origin->getX() > destination->getX() ){
            return "LEFT";
        }else{
            return "RIGHT";
        }
    }
}


void gameLoop(Board& board, MCTS& mcts){
    vector<Player> enemies;
    Player player;

    // THE FRIST POSITION IS NOT MARKED AS BLOCKED

    // game loop
    while (1) {
        int N; // total number of players (2 to 4).
        int P; // your player number (0 to 3).
        cin >> N >> P; cin.ignore();
        for (int i = 0; i < N; i++) {
            int X0; // starting X coordinate of lightcycle (or -1)
            int Y0; // starting Y coordinate of lightcycle (or -1)

            int X1; // starting X coordinate of lightcycle (can be the same as X0 if you play before this player)
            int Y1; // starting Y coordinate of lightcycle (can be the same as Y0 if you play before this player)

            cin >> X0 >> Y0 >> X1 >> Y1; cin.ignore();


            if( X0 != -1 && Y0 != -1 ){
                board.setBlocked(board.getPoint(X0, Y0));
            }
            board.setBlocked(board.getPoint(X1, Y1));
            // This player is me
            if(i == P && enemies.size() != N){
                player = Player(board.getPoint(X1, Y1));
            }else if( i == P ){
                player.position = board.getPoint(X1, Y1);
            }else{
                if (enemies.size() != N){
                    enemies.push_back(Player(board.getPoint(X1, Y1)));
                }else {
                    enemies.at(i).position = board.getPoint(X1, Y1);
                }
            }
        }

        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;
        Point* result = mcts.getBestChoice(board, enemies, player);

        cerr << "Amount of blocked points " << board.getBlocked().size() << endl;

        // IF no valid path exists
        if( result->getX() == -1 ){
            cout << "No Move" << endl;
        }else{
            cout << getDirection(player.position, result) << endl;
        }
    }
}

void init(){
    for(int x = 0; x < width; x++){
        vector<Point*> yVector;
        for(int y = 0; y < height; y++){
            Point* p = new Point(x, y);
            yVector.push_back(p);
        }
        points.push_back(yVector);
    }
}


int main() {
    init();

    MCTS mcts = MCTS();
    Board board = Board();


    if(TESTING){
        Player enemy =  Player(board.getPoint(10, 10));
        vector<Player> enemies = { enemy };
        Player player = Player(board.getPoint(15, 15));

        for(int i = 0; i < 13; i++){
            board.setBlocked(player.position);
            board.setBlocked(enemy.position);

            Point* result = mcts.getBestChoice(board, enemies, player);
            cout << "The best move is: (" << result->getX() << ", " << result->getY() << ")" << std::endl;
            cout << "Direction: " << getDirection(player.position, result) << " FROM (" << player.position->getX() << ", " << player.position->getY() << ") TO (" << result->getX() << ", " << result->getY() << ")" << std::endl;


            player.position = result;
        }

    }else{
        gameLoop(board, mcts);
    }



    return 0;
}