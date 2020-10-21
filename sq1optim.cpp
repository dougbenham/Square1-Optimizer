#include <stdafx.h>
#include <iostream>

using namespace std;

#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUMHALVES 13
#define NUMLAYERS 158
#define NUMSHAPES 7356
#define FILESTT "sq1stt.dat"
#define FILESCTE "sq1scte.dat"
#define FILESCTC "sq1sctc.dat"
#define FILEP1U  "sq1p1u.dat"
#define FILEP2U  "sq1p2u.dat"
#define FILEP1W  "sq1p1w.dat"
#define FILEP2W  "sq1p2w.dat"

const char* errors[]={
	"Unrecognised command line switch.", //1
	"Too many command line arguments.",
	"Input file not found.",//3
	"Bracket ) expected.",//4
	"Bottom layer turn expected.",//5
	"Comma expected.",//6
	"Top layer turn expected.",//7
	"Bracket ( expected.",//"8
	"Position should be 16 or 17 characters.",//9
	"Expected A-H or 1-8.",//10
	"Expected - or /.",//11
	"Twist is blocked by corner.",//12
	"Unrecognised argument.",//13
	"Unexpected bracket (.",//14
};

class HalfLayer {
public:
	int pieces, turn, nPieces;
	HalfLayer(int p, int t) {
		int i,m=1,nEdges=0;
		pieces = p;
		for(i=0; i<6; i++){
			if( (pieces&m)!=0 ) nEdges++;
			m<<=1;
		}
		nPieces=3+nEdges/2;
		turn=t;
	}
};

class Layer {
public:
	HalfLayer& h1, & h2;
	int turnt, turnb;
	int nPieces;
	bool turnParityOdd;
	bool turnParityOddb;
	int pieces;
	int tpieces, bpieces;   // result after turn

	Layer( HalfLayer& p1, HalfLayer& p2): h1(p1), h2(p2) {
		pieces = (h1.pieces<<6)+h2.pieces;
		nPieces = h1.nPieces + h2.nPieces;

		int m=1;
		for(turnt=1; turnt<6; turnt++){
			if( (h1.turn&h2.turn&m)!=0 ) break;
			m<<=1;
		}
		if( turnt==6 ) turnb=6;
		else{
			m=1<<4;
			for(turnb=1; turnb<5; turnb++){
				if( (h1.turn&h2.turn&m)!=0 ) break;
				m>>=1;
			}
		}

		tpieces=pieces;
		int i, nEdges=0;
		for( i=0; i<turnt; i++ ){
			if( (tpieces&1)!=0 ) { tpieces+=(1<<12); nEdges++; }
			tpieces>>=1;
		}
		//find out parity of that layer turn
		// Is odd cycle if even # pieces, and odd number passes seam
		//  Note (turn+edges)/2 = number of pieces crossing seam
		turnParityOdd = (nPieces&1)==0 && ((turnt+nEdges)&2)!=0;

		bpieces=pieces;
		nEdges=0;
		for( i=0; i<turnb; i++ ){
			bpieces<<=1;
			if( (bpieces&(1<<12))!=0 ) { bpieces-=(1<<12)-1; nEdges++; }
		}
		//find out parity of that layer turn
		// Is odd cycle if even # pieces, and odd number passes seam
		//  Note (turn+edges)/2 = number of pieces crossing seam
		turnParityOddb = (nPieces&1)==0 && ((turnb+nEdges)&2)!=0;

	}
};

class Sq1Shape {
public:
	Layer& topl, &botl;
	int pieces;
	bool parityOdd;
	int tpieces[4];
	bool tparity[4];
	Sq1Shape( Layer& l1, Layer& l2, bool p) : topl(l1), botl(l2) {
		parityOdd=p;
		pieces = (l1.pieces<<12)+l2.pieces;
		tpieces[0] = (l1.tpieces<<12)+l2. pieces;
		tpieces[1] = (l1. pieces<<12)+l2.bpieces;
		tpieces[2] = (l1.h1.pieces<<18)+(l2.h1.pieces<<12)+(l1.h2.pieces<<6)+(l2.h2.pieces);
		// calculate mirrored shape
		tpieces[3] = 0;
		int i,m;
		for( m=1, i=0; i<24; i++,m<<=1){
			tpieces[3]<<=1;
			if( (pieces&m)!=0 ) tpieces[3]++;
		}
		tparity[0] = parityOdd^l1.turnParityOdd;
		tparity[1] = parityOdd^l2.turnParityOddb;
		tparity[2] = parityOdd^( (l1.h2.nPieces&1)!=0 && (l2.h1.nPieces&1)!=0 );
		tparity[3] = parityOdd;
	}
};


class ChoiceTable {
public:
	unsigned char choice2Idx[256];
	unsigned char idx2Choice[70];
	ChoiceTable(){
		unsigned char nc=0;
		int i,j,k,l;
		for( i=0; i<255; i++ ) choice2Idx[i]=255;
		for( i=1; i<255; i<<=1 ){
			for( j=i+i; j<255; j<<=1 ){
				for( k=j+j; k<255; k<<=1 ){
					for( l=k+k; l<255; l<<=1 ){
						choice2Idx[i+j+k+l]=nc;
						idx2Choice[nc++]=i+j+k+l;
					}
				}
			}
		}
	}
};


class ShapeTranTable {
public:
	int nShape;
	Sq1Shape* shapeList[NUMSHAPES];
	int (*tranTable)[4];
	HalfLayer* hl[NUMHALVES];
	Layer* ll[NUMLAYERS];

	ShapeTranTable(){
		int i,j,m;
		//first build list of possible halflayers
		int hi[]={ 0,    3,12,48, 9,36,33,  15,39,51,57,60,  63};
		int ht[]={42,   43,46,58,45,54,53,  47,55,59,61,62,  63};
		for( i=0; i<NUMHALVES; i++ ){ hl[i]=new HalfLayer(hi[i],ht[i]); }

		//Now build list of possible Layers
		int lll=0;
		for( i=0; i<NUMHALVES; i++ ){
			for( j=0; j<NUMHALVES; j++ ){
				if( hl[i]->nPieces + hl[j]->nPieces<=10 ){
					ll[lll++]=new Layer( *hl[i], *hl[j] );
				}
			}
		}

		//Now build list of all possible shapes
		nShape=0;
		for( i=0; i<lll; i++ ){
			for( j=0; j<lll; j++ ){
				if( ll[i]->nPieces + ll[j]->nPieces==16 ){
					shapeList[nShape++]=new Sq1Shape( *ll[i], *ll[j], true );
					shapeList[nShape++]=new Sq1Shape( *ll[i], *ll[j], false );
				}
			}
		}

		// At last we can calculate full transition table
		tranTable = new int[NUMSHAPES][4];
		// see if can be found on file
		ifstream is(FILESTT, ios::binary);
		if( is.fail() ){
			// no file. calculate table.
			for( i=0; i<nShape; i++ ){
				//effect on shape of each move, incuding reflection
				for( m=0; m<4; m++ ){
					for( j=0; j<nShape; j++ ){
						if( shapeList[i]->tpieces[m] == shapeList[j]->pieces &&
							shapeList[i]->tparity[m] == shapeList[j]->parityOdd ){
								tranTable[i][m]=j;
								break;
						}
					}
					if( j>=nShape ){
						exit(0);
					}
				}
			}
			// save to file
			ofstream os(FILESTT, ios::binary);
			os.write( (char*)tranTable, nShape*4*sizeof(int) );
		}else{
			// read from file
			nShape = NUMSHAPES;
			is.read( (char*)tranTable, nShape*4*sizeof(int) );
		}
	}
	~ShapeTranTable(){
		int i;
		for( i=0; i<NUMHALVES; i++ ){ delete hl[i]; }
		for( i=0; i<NUMLAYERS; i++ ){ delete ll[i]; }
		for( i=0; i<nShape; i++ ){ delete shapeList[i]; }
		delete[] tranTable;
	}
	inline int getShape(int s, bool p){
		int i;
		for( i=0; i<nShape; i++){
			if( shapeList[i]->pieces == s && shapeList[i]->parityOdd==p ) return i;
		}
		return -1;
	}
	inline int getTopTurn(int s){
		return shapeList[s]->topl.turnt;
	}
	inline int getBotTurn(int s){
		return shapeList[s]->botl.turnb;
	}
};

class ShapeColPos {
	ShapeTranTable &stt;
	ChoiceTable &ct;
	int shapeIx;
	int colouring; //24bit string
	bool edgesFlag;
public:
	ShapeColPos( ShapeTranTable& stt0, ChoiceTable& ct0)
		: stt(stt0), ct(ct0) {}
	void set( int shp, int col, bool edges )
	{
		// col is 8 bit colouring of one type of piece.
		// edges set then edge colouring, else corner colouring
		// get full 24 bit colouring.
		int i,m,n=1,s;
		int c=ct.idx2Choice[col];
		shapeIx = shp;
		edgesFlag = edges;
		colouring=0;
		s=stt.shapeList[shapeIx]->pieces;
		if( edges ){
			for( m=1, i=0; i<24; m<<=1, i++){
				if( (s&m)!=0 ) {
					if( (c&n)!=0 ) colouring |= m;
					n<<=1;
				}
			}
		}else{
			for( m=3, i=0; i<24; m<<=1, i++){
				if( (s&m)==0 ) {
					if( (c&n)!=0 ) colouring |= m;
					n<<=1;
					m<<=1; i++;
				}
			}
		}
	}
	void domove(int m){
		const int botmask = (1<<12)-1;
		const int topmask = (1<<24)-(1<<12);
		const int botrmask = (1<<12)-(1<<6);
		const int toprmask = (1<<18)-(1<<12);
		const int leftmask = botmask+topmask-botrmask-toprmask;
		int b,t,tn=0;
		if( m==0 ){
			tn=stt.getTopTurn(shapeIx);
			b=colouring&botmask;
			t=colouring&topmask;
			t+=(t>>12);
			t<<=(12-tn);
			colouring = b + (t&topmask);
		}else if( m==1 ){
			tn=stt.getBotTurn(shapeIx);
			b=colouring&botmask;
			t=colouring&topmask;
			b+=(b<<12);
			b>>=(12-tn);
			colouring = t + (b&botmask);
		}else if( m==2 ){
			b=colouring&botrmask;
			t=colouring&toprmask;
			colouring = (colouring&leftmask) + (t>>6) + (b<<6);
		}
		shapeIx=stt.tranTable[shapeIx][m];
	}
	unsigned char getColIdx(){
		int i,c=0,m,s,n=1;
		s=stt.shapeList[shapeIx]->pieces;
		if( edgesFlag ){
			for( m=1, i=0; i<24; m<<=1, i++){
				if( (s&m)!=0 ) {
					if( (colouring&m)!=0 ) c |= n;
					n<<=1;
				}
			}
		}else{
			for( m=3, i=0; i<24; m<<=1, i++){
				if( (s&m)==0 ) {
					if( (colouring&m)!=0 ) c |= n;
					n<<=1;
					m<<=1; i++;
				}
			}
		}
		return(ct.choice2Idx[c]);
	}
};



class ShpColTranTable {
public:
	char (*tranTable)[70][3];
	ShapeTranTable& stt;
	ChoiceTable& ct;

	ShpColTranTable( ShapeTranTable& stt0, ChoiceTable& ct0, bool edges )
		: stt(stt0), ct(ct0)
	{
		ShapeColPos p(stt,ct);
		tranTable = new char[NUMSHAPES][70][3];

		// see if can be found on file
		ifstream is( edges? FILESCTE : FILESCTC, ios::binary);
		if( is.fail() ){
			// no file. calculate table.
			// Calculate transition table
			int i,j,m;
			for( m=0; m<3; m++ ){
				for( i=0; i<NUMSHAPES; i++ ){
					for( j=0; j<70; j++){
						p.set(i,j,edges);
						p.domove(m);
						tranTable[i][j][m]=p.getColIdx();
						if( p.getColIdx()==255 ){
							exit(0);
						}
					}
				}
			}
			// save to file
			ofstream os(edges? FILESCTE : FILESCTC, ios::binary);
			os.write( (char*)tranTable, NUMSHAPES*3*70*sizeof(char) );
		}else{
			// read from file
			is.read( (char*)tranTable, NUMSHAPES*3*70*sizeof(char) );
		}
	}
	~ShpColTranTable(){
		delete[] tranTable;
	}
};

// FullPosition holds position with each piece individually specified.
class FullPosition {
public:
	int pos[24];
	int middle;
	FullPosition(){ reset(); }
	void reset(){
		middle=1;
		for( int i=0; i<24; i++)
			pos[i]="AAIBBJCCKDDLMEENFFOGGPHH"[i]-'A';
	}
	char* getvalue(){
		char* var = new char[24];
		int index = 0;
		for(int i=0; i<24; i++)
		{
			var[index] = "ABCDEFGH12345678"[pos[i]];
			if(pos[i] < 8) i++;
			index++;
		}
		var[index] = 0;
		return var;
	}
	void print(){
		for(int i=0; i<24; i++){
			cout<<"ABCDEFGH12345678"[pos[i]];
			if( pos[i]<8 ) i++;
		}
		cout<<"/ -"[middle+1]<<endl<<flush;
	}
	void random(){
		int i,j,k,tmp[16];
		middle = (rand()&1)!=0?-1:1;
		do{
			//mix array
			for( i=0;i<16; i++) tmp[i]=i;
			for( i=0;i<16; i++){
				j=rand()%(16-i);
				k=tmp[i];tmp[i]=tmp[i+j];tmp[i+j]=k;
			}
			//store
			for(i=j=0;i<16;i++){
				pos[j++]=tmp[i];
				if( tmp[i]<8 ) pos[j++]=tmp[i];
			}
			// test twistable
		}while( pos[5]==pos[6] || pos[11]==pos[12] || pos[17]==pos[18] );
	}
	void set(int p[],int m){
		for(int i=0;i<24;i++)pos[i]=p[i];
		middle=m;
	};

	void doTop(int m){
		m %= 12;
		if (m < 0)
			m += 12;

		while(m>0){
			int c=pos[11];
			for(int i=11;i>0;i--) pos[i]=pos[i-1];
			pos[0]=c;
			m--;
		}
	}
	void doBot(int m){
		m %= 12;
		if (m < 0)
			m += 12;

		while(m>0){
			int c=pos[23];
			for(int i=23;i>12;i--) pos[i]=pos[i-1];
			pos[12]=c;
			m--;
		}
	}

	bool doTwist()
	{
		if (!isTwistable())
			return false;

		for(int i = 6; i < 12; i++)
		{
			int c = pos[i];
			pos[i] = pos[i + 6];
			pos[i + 6] = c;
		}

		middle = -middle;

		return true;
	}
	bool isTwistable()
	{
		return( pos[0]!=pos[11] && pos[5]!=pos[6] && pos[12]!=pos[23] && pos[17]!=pos[18] );
	}
	int getShape(){
		int i,m,s=0;
		for(m=1<<23,i=0; i<24; i++,m>>=1){
			if(pos[i]>=8) s|=m;
		}
		return(s);
	}
	bool getParityOdd(){
		int i,j;
		bool p=false;
		for(i=0; i<24; i++){
			for(j=i; j<24; j++){
				if( pos[j]<pos[i]) p=!p;
				if(pos[j]<8)j++;
			}
			if(pos[i]<8)i++;
		}
		return(p);
	}
	int getEdgeColouring(int cl){
		const int clp[3][4]={ { 8, 9,10,11}, { 8, 9,13,14}, {15,14,10, 9} };
		int i,j,c=0;
		int m=(cl!=2)?1<<7:1;
		for(i=0; i<24; i++){
			if( pos[i]>=8 ){
				for(j=0; j<4; j++){
					if( pos[i]==clp[cl][j] ){
						c|=m;
						break;
					}
				}
				if(cl!=2) m>>=1; else m<<=1;
			}
		}
		return(c);
	}
	int getCornerColouring(int cl){
		const int clp[3][4]={ {0,1,2,3}, {0,1,5,6}, {7,6,2,1} };
		int i,j,c=0;
		int m=(cl!=2)?1<<7:1;
		for(i=0; i<24; i++){
			if( pos[i]<8 ){
				for(j=0; j<4; j++){
					if( pos[i]==clp[cl][j] ){
						c|=m;
						break;
					}
				}
				if(cl!=2) m>>=1; else m<<=1;
				i++;
			}
		}
		return(c);
	}
	int parseInput( const char* inp )
	{
		// scan characters
		const char* t=inp;
		int f=0;
		while(*t){
			if( *t == ',' || *t == '(' || *t == ')' || *t == '9' || *t == '0' ){
				f|=1; // cannot be position string, but may be movelist
			}else if( (*t>='a' && *t<='h') || (*t>='A' && *t<='H') ){
				f|=2; // cannot be movelist, but may be position string
			}else if( *t!='/' && *t!='-' && (*t<'1' || *t>'8') ){
				f|=3; // cannot be either
			}
			t++;
		}
		if( f==3 || f==0 || f==1 ){
			return(13);
		}

		reset();
		int lw=0,lu=0;
		// position
		if( strlen(inp)!=16 && strlen(inp)!=17 ) return(9);
		int j=0;
		int pi[24];
		for( int i=0; i<16; i++){
			int k=inp[i];
			if(k>='a' && k<='h') k-='a';
			else if(k>='A' && k<='H') k-='A';
			else if(k>='1' && k<='8') k-='1'-8;
			else return(10);
			pi[j++] = k;
			if( k<8) pi[j++] = k;
		}
		int midLayer=0;
		if( strlen(inp)==17 ){
			int k=inp[16];
			if( k!='-' && k!='/' ) return(11);
			midLayer = (k=='-') ? 1 : -1;
		}
		set(pi,midLayer);
		return(0);
	}
};

//pruning table for combination of shape,edgecolouring,cornercolouring.
class PrunTable {
public:
	char (*table)[70][70];
	ShapeTranTable& stt;
	ShpColTranTable& scte;
	ShpColTranTable& sctc;

	PrunTable( FullPosition& p0, int cl, ShapeTranTable& stt0, ShpColTranTable& scte0, ShpColTranTable& sctc0, bool turnMetric )
		: stt(stt0), scte(scte0), sctc(sctc0)
	{
		// Calculate pruning table
		int i0,i1,i2,j0,j1,j2,m,n,w;
		char l;
		const char *fname;
		table = new char[NUMSHAPES][70][70];
		if( turnMetric ){
			fname = (cl==0)? FILEP1U : FILEP2U;
		}else{
			fname = (cl==0)? FILEP1W : FILEP2W;
		}

		// see if can be found on file
		ifstream is( fname, ios::binary );
		if( is.fail() ){
			// no file. calculate table.
			// clear table
			for( i0=0; i0<NUMSHAPES; i0++ ){
				for( i1=0; i1<70; i1++){
					for( i2=0; i2<70; i2++){
						table[i0][i1][i2]=0;
					}}}
			//set start position
			int s0 = stt.getShape(p0.getShape(),p0.getParityOdd());
			int e0 = p0.getEdgeColouring(cl);
			int c0 = p0.getCornerColouring(cl);
			e0 = scte0.ct.choice2Idx[e0];
			c0 = sctc0.ct.choice2Idx[c0];
			if( turnMetric ){
				table[s0][e0][c0]=1;
			}else{
				setAll(s0,e0,c0,1);
			}

			l=1;
			do{
				n=0;
				if( turnMetric ){
					for( i0=0; i0<NUMSHAPES; i0++ ){
						for( i1=0; i1<70; i1++){
							for( i2=0; i2<70; i2++){
								if( table[i0][i1][i2]==l ){
									for( m=0; m<3; m++){
										j0=i0;j1=i1;j2=i2;
										w=0;
										do{
											j2=sctc.tranTable[j0][j2][m];
											j1=scte.tranTable[j0][j1][m];
											j0=stt.tranTable[j0][m];
											if( table[j0][j1][j2]==0 ){
												table[j0][j1][j2]=l+1;
												n++;
											}
											w++;
											if(w>12){
												exit(0);
											}
										}while(j0!=i0 || j1!=i1 || j2!=i2 );
									}
								}
							}}}
				}else{
					for( i0=0; i0<NUMSHAPES; i0++ ){
						for( i1=0; i1<70; i1++){
							for( i2=0; i2<70; i2++){
								if( table[i0][i1][i2]==l ){
									// do twist
									j0=stt.tranTable[i0][2];
									j1=scte.tranTable[i0][i1][2];
									j2=sctc.tranTable[i0][i2][2];
									if( table[j0][j1][j2]==0 ){
										n+=setAll(j0,j1,j2,l+1);
									}
								}
							}}}
				}
				cout<<"     l="<<(int)l<<"  n="<<(int)n<<endl<<flush;
				l++;
			}while(n!=0);

			// save to file
			ofstream os( fname, ios::binary );
			os.write( (char*)table, NUMSHAPES*70*70*sizeof(char) );
		}else{
			// read from file
			is.read( (char*)table, NUMSHAPES*70*70*sizeof(char) );
		}


	}
	~PrunTable(){
		delete[] table;
	}
	// set a position to depth l, as well as all rotations of it.
	inline int setAll(int i0,int i1,int i2, char l){
		int j0,j1,j2,k0,k1,k2,n=0;
		j0=i0; j1=i1; j2=i2;
		do{
			k0=j0;k1=j1;k2=j2;
			do{
				if( table[k0][k1][k2]==0 ){
					table[k0][k1][k2]=l;
					n++;
				}
				k2=sctc.tranTable[k0][k2][0];
				k1=scte.tranTable[k0][k1][0];
				k0=stt.tranTable[k0][0];
			}while(j0!=k0 || j1!=k1 || j2!=k2 );
			j2=sctc.tranTable[j0][j2][1];
			j1=scte.tranTable[j0][j1][1];
			j0=stt.tranTable[j0][1];
		}while(j0!=i0 || j1!=i1 || j2!=i2 );
		return n;
	}
};


// SimpPosition holds position encoded by colourings
class SimpPosition {
	int e0,e1,e2,c0,c1,c2;
	int shp,shp2,middle;
	ShapeTranTable& stt;
	ShpColTranTable& scte;
	ShpColTranTable& sctc;
	PrunTable& pr1;
	PrunTable& pr2;
	FullPosition fullpos;

	int moveList[50];
	int moveLen;
	int lastTurns[6];
	bool turnMetric;
	bool findAll;
	bool ignoreTrans;

public:
	SimpPosition( ShapeTranTable& stt0, ShpColTranTable& scte0, ShpColTranTable& sctc0, PrunTable& pr10, PrunTable& pr20 )
		: stt(stt0), scte(scte0), sctc(sctc0), pr1(pr10), pr2(pr20) { };
	void set(FullPosition& p, bool turnMetric0, bool findAll0, bool ignoreTrans0){
		fullpos = p;
		c0=sctc.ct.choice2Idx[p.getCornerColouring(0)];
		c1=sctc.ct.choice2Idx[p.getCornerColouring(1)];
		c2=sctc.ct.choice2Idx[p.getCornerColouring(2)];
		e0=scte.ct.choice2Idx[p.getEdgeColouring(0)];
		e1=scte.ct.choice2Idx[p.getEdgeColouring(1)];
		e2=scte.ct.choice2Idx[p.getEdgeColouring(2)];
		shp = stt.getShape(p.getShape(),p.getParityOdd());
		shp2 = stt.tranTable[shp][3];
		middle = p.middle;
		turnMetric=turnMetric0;
		findAll=findAll0;
		ignoreTrans=ignoreTrans0;
	};
	int doMove(int m){
		const int mirrmv[3]={1,0,2};
		int r=0;
		if(m==0){
			r=stt.getTopTurn(shp);
			fullpos.doTop(r);
		}else if(m==1){
			r=stt.getBotTurn(shp);
			fullpos.doBot(-r);
		}else{
			middle=-middle;
			fullpos.doTwist();
		}
		c0 = sctc.tranTable[shp][c0][m];
		c1 = sctc.tranTable[shp][c1][m];
		e0 = scte.tranTable[shp][e0][m];
		e1 = scte.tranTable[shp][e1][m];
		shp = stt.tranTable[shp][m];

		c2 = sctc.tranTable[shp2][c2][mirrmv[m]];
		e2 = scte.tranTable[shp2][e2][mirrmv[m]];
		shp2 = stt.tranTable[shp2][mirrmv[m]];
		return r;
	}
	void solve(bool r, char* targetState, bool useTopTurns, bool useBotTurns){
		int l=-1;
		moveLen=0;
		unsigned long nodes=0;
		// only even lengths if twist metric and middle is square
		if( !turnMetric && middle==1 ) l=-2;
		// do ida
		bool result = true;
		do
		{
			l++;
			if( !turnMetric && middle!=0 ) l++;
			if(!r) cout<<"Searching depth "<<l<<".."<<endl<<flush;
			for( int i=0; i<6; i++) lastTurns[i]=0;
			if (search(l,3, &nodes, targetState, useTopTurns, useBotTurns) > 0)
			{
				cout<<"Continue searching? (type 'n' then press enter to stop, otherwise just press enter)  "<<flush;
				if (getchar() == 'n')
					result = false;
				cout<<endl<<flush;
			}
		}
		while( result );
	}
	int search( const int l, const int lm, unsigned long *nodes, char* targetState, bool useTopTurns, bool useBotTurns){
		int i,r=0;

		// search for l more moves. previous move was lm.
		(*nodes)++;
		if( l<0 ) return 0;

		//prune based on transformation
		// (a,b)/(c,d)/(e,f) -> (6+a,6+b)/(d,c)/(6+e,6+f)
		if( turnMetric && !ignoreTrans ){
			// (a,b)/(c,d)/(e,f) -> (6+a,6+b)/(d,c)/(6+e,6+f)
			// moves changes by:
			// a,b,e,f=0/6 -> m++/m--
			i=0;
			if( lastTurns[0]==0 ) i++;
			else if( lastTurns[0]==6 ) i--;
			if( lastTurns[1]==0 ) i++;
			else if( lastTurns[1]==6 ) i--;
			if( lastTurns[4]==0 ) i++;
			else if( lastTurns[4]==6 ) i--;
			if( lastTurns[5]==0 ) i++;
			else if( lastTurns[5]==6 ) i--;
			if( i<0 || ( i==0 && lastTurns[0]>=6 ) ) return(0);
		}
			
		// check if it is now solved
		if( l==0 ){

			//if( shp==4163 && e0==69 && e1==44 && e2==44 && c0==69 && c1==44 && c2==44 && middle>=0 ){
			
			char* value = fullpos.getvalue();
			bool equals = true;
			for (int i = 0; i < 16; i++)
			{
				switch (targetState[i])
				{
					case 'X':
					case 'x':
						// Edge
						equals = equals && (value[i] >= '1' && value[i] <= '8');
						break;
					case 'Y':
					case 'y':
						// Corner
						equals = equals && (value[i] >= 'A' && value[i] <= 'H');
						break;
					case 'K':
					case 'k':
						// U edge
						equals = equals && (value[i] >= '1' && value[i] <= '4');
						break;
					case 'L':
					case 'l':
						// D edge
						equals = equals && (value[i] >= '5' && value[i] <= '8');
						break;
					case 'M':
					case 'm':
						// U corner
						equals = equals && (value[i] >= 'A' && value[i] <= 'D');
						break;
					case 'N':
					case 'n':
						// D corner
						equals = equals && (value[i] >= 'E' && value[i] <= 'H');
						break;
					default:
						// Exact match
						equals = equals && value[i] == "A1B2C3D45E6F7G8H"[i];
						break;
				}
			}
			delete[] value;
			
			if( shp==4163 && equals && middle>=0 ){
				printsol(*nodes);
				return 1;
			}else if( turnMetric ) return 0;
		}

		// prune
		if( pr1.table[shp ][e0][c0]>l+3 ) return(0);
		if( pr2.table[shp ][e1][c1]>l+3 ) return(0);
		if( pr2.table[shp2][e2][c2]>l+3 ) return(0);

		// try all top layer moves
		if( lm>=2 && useTopTurns ){
			i=doMove(0);
			do{
				if( turnMetric || ignoreTrans || i<6 || l<2 ){
					moveList[moveLen++]=i;
					lastTurns[4]=i;
					r+=search( turnMetric?l-1:l, 0, nodes, targetState, useTopTurns, useBotTurns);
					moveLen--;
					if(r!=0 && !findAll) return(r);
				}
				i+=doMove(0);
			}while( i<12);
			lastTurns[4]=0;
		}
		// try all bot layer moves
		if( lm!=1 && useBotTurns ){
			i=doMove(1);
			do{
				moveList[moveLen++]=i+12;
				lastTurns[5]=i;
				r+=search( turnMetric?l-1:l, 1, nodes, targetState, useTopTurns, useBotTurns);
				moveLen--;
				if(r!=0 && !findAll) return(r);
				i+=doMove(1);
			}while( i<12);
			lastTurns[5]=0;
		}
		// try twist move
		if( lm!=2 ){
			int lt0=lastTurns[0], lt1=lastTurns[1];
			lastTurns[0]=lastTurns[2];
			lastTurns[1]=lastTurns[3];
			lastTurns[2]=lastTurns[4];
			lastTurns[3]=lastTurns[5];
			lastTurns[4]=0;
			lastTurns[5]=0;
			doMove(2);
			moveList[moveLen++]=0;
			r+=search(l-1, 2, nodes, targetState, useTopTurns, useBotTurns);
			if(r!=0 && !findAll) return(r);
			moveLen--;
			doMove(2);
			lastTurns[5]=lastTurns[3];
			lastTurns[4]=lastTurns[2];
			lastTurns[3]=lastTurns[1];
			lastTurns[2]=lastTurns[0];
			lastTurns[1]=lt1;
			lastTurns[0]=lt0;
		}
		return r;
	}
	void printsol(unsigned long nodes){
		int tw=0, tu=0;
		int lm=3;
		for( int i=0; i<moveLen; i++){
			if( moveList[i]==0 ){
				if( lm==0 ) cout<<"0";
				cout<<"/";
				tu++; tw++;
				lm=2;
			}else if( moveList[i]<12 ){
				cout<<moveList[i]<<",";
				tu++;
				lm=0;
			}else{
				if(lm>=2) cout<<"0,";
				cout<<(24-moveList[i]);
				tu++;
				lm=1;
			}
		}
		if(lm==0) cout<<"0";
		cout <<"  ["<<tw<<"|"<<tu<<"] "<<endl;
		//cout<<"Nodes="<<nodes<<endl<<flush;

	}
};

int show(int e){
	cout<<errors[e-1]<<endl<<flush;
	return(e);
}

// -w|u=twist/turn metric  -a=all  -m=ignore middle
int main(int argc, char* argv[]){
	
	bool ignoreMid=false;
	bool ignoreTrans=false;
	bool turnMetric=true;
	bool findAll=false;
	bool repeat=false;
	bool useTopTurns=true;
	bool useBotTurns=true;
	char* targetState = "----------------"; // 16 -'s
	char *inpFile=NULL;
	int posArg=-1;
	for( int i=1; i<argc; i++){
		if( argv[i][0]=='-' ){
			switch (argv[i][1]){
			case 'w':
			case 'W':
				turnMetric=false; break;
			case 'u':
			case 'U':
				turnMetric=true; break;
			case 'x':
			case 'X':
				ignoreTrans=true; break;
			case 'a':
			case 'A':
				findAll=true; break;
			case 'm':
			case 'M':
				ignoreMid=true; break;
			case 'r':
			case 'R':
				repeat=true; break;
			case 't':
			case 'T':
				targetState = argv[i + 1];
				i++;
				break;
			case 'o':
				if (strcmpi(argv[i], "-onlytop") == 0)
				{
					useBotTurns = false;
					useTopTurns = true;
				}
				else if (strcmpi(argv[i], "-onlybottom") == 0)
				{
					useTopTurns = false;
					useBotTurns = true;
				}
				break;
			case 'i':
			case 'I':
				inpFile=argv[i]+2; break;
			default:
				return show(1);
			}
		}else if( posArg<0 ){
			posArg = i;
		}else{
			return show(2);
		}
	}

	FullPosition p;
	ifstream is;
	// parse position/move sequence from argument posArg
	if( posArg>=0 ){
		int r=p.parseInput(argv[posArg]);
		if(r) return show(r);
	}else if( inpFile!=NULL ){
		is.open(inpFile);
		if(is.fail()) return show(3);
	}
	// now we have a position p to solve

	cout << "Initializing..." << endl;
	// calculate transition tables
	ChoiceTable ct;
	cout << "  5. Shape transition table" << endl;
	ShapeTranTable st;
	cout << "  4. Colouring 1 transition table" << endl;
	ShpColTranTable scte( st, ct, true );
	cout << "  3. Colouring 2 transition table" << endl;
	ShpColTranTable sctc( st, ct, false );

	//calculate pruning tables for two colourings
	FullPosition q;
	cout << "  2. Colouring 1 pruning table" << endl;
	PrunTable pr1(q, 0, st,scte,sctc, turnMetric );
	cout << "  1. Colouring 2 pruning table" << endl;
	PrunTable pr2(q, 1, st,scte,sctc, turnMetric );
	cout << "  0. Finished." << endl << endl;
	SimpPosition s( st, scte, sctc, pr1, pr2 );

	cout<<"Flags: "<<(turnMetric? "Turn":"Twist")<<" Metric, Find ";
	cout<< (findAll? "All Solutions":"Only First Solution")<<endl;

	srand( (unsigned)time( NULL ) );
	char buffer[200];
	do{
		if( posArg<0 ){
			if( inpFile!=NULL ){
				is.getline(buffer,199);
				int r=p.parseInput(buffer);
				if(r) {
					show(r);
					continue;
				}
			}else{
				p.random();
			}
		}
		if( ignoreMid ) p.middle=0;

		//show position
		cout<<"Position to solve: ";
		p.print();

		// convert position to colour encoding
		s.set(p, turnMetric, findAll, ignoreTrans);

		//solve position
		s.solve(repeat, targetState, useTopTurns, useBotTurns);
	}while( posArg<0 && ( (inpFile==NULL && repeat) || (inpFile!=NULL && !is.eof() ) ));

	cout<<"Press enter to exit.."<<flush;
	getchar();
	return(0);
}





/*

ttshp: 7356*3 ints.   done

tt: 70*7356*3 chars for edges
tt: 70*7356*3 chars for corners

pt: 70*70*7356 chars colour 1,2
pt: 70*70*7356 chars colour 3

*/
