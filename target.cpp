using namespace std;
#include <string>
#include <vector>
#include <map>

#include <string.h>
#include "target.h"

// remplir un listing a partir de l'adresse donnee, jusqu'a epuisement du disass
// le contenu anterieur est ecrase
// si le disass n'est pas dispo le listing est laisse vide
// si ilist ne pointe pas sur un listing, retour -1
int target::fill_listing( unsigned int ilist, unsigned long long adr )
{
if	( ilist >= liststock.size() )
	return -1;
unsigned int ia, delta;
int src0, srci;
asmline * daline;
listing * curlist = &(liststock[ilist]);
curlist->lines.clear();
curlist->adr0 = adr;
while	( asmmap.count(adr) )
	{
	// lire la ligne desassemblee
	ia = asmmap[adr];
	daline = &(asmstock[ia]);
	delta = daline->qbytes;
	if	( delta == 0 )
		break;		// ligne invalide, impossible continuer
	// traiter eventuelles lignes de src
	src0 = daline->src0;
	if	( src0 >= 0 )
		{
		for	( srci = src0; srci <= daline->src1; ++srci )
			{
			curlist->lines.push_back( listing::encode_ref( daline->isrc, (unsigned int)srci ) );
			}
		}
	// completer listing avec la ligne asm
	curlist->lines.push_back((int)ia);
	// passer a l'instruction suivante
	adr += delta;
	}
// le listing est fini
curlist->adr1 = adr;
return 0;
}

// retourne index du listing ou -1 si echec
int target::add_listing( unsigned long long adr )		// not tested
{
if	( asmmap.count(adr) == 0 )
	return -1;
// creer listing vierge
listing newlist;
liststock.push_back( newlist );
unsigned int ilist = liststock.size() - 1;
fill_listing( ilist, adr );
return (int)ilist;
}

// formatte une valeur lue dans une RAM
int target::ram_val2txt( char * text, unsigned int size, unsigned int iram, unsigned int iline, int ram_format ) {
const char * fmt;
if	( iram >= ramstock.size() )
	return snprintf( text, size, "no data" );
switch	( ram_format )
	{
	case 64 :
		if	( (1+(2*iline)) < ramstock[iram].w32.size() )
			{
			fmt = "%08X%08X";
			return snprintf( text, size, fmt, ramstock[iram].w32[1+(2*iline)], ramstock[iram].w32[2*iline] );
			}
		else	return snprintf( text, size, "no data" );
		break;
	case 65 :	// 64 bits, display ascii 8 chars
		if	( (1+(2*iline)) < ramstock[iram].w32.size() )
			{
			unsigned char * b; unsigned int i;
			b = (unsigned char *)&(ramstock[iram].w32[2*iline]);
			for	( i = 0; (i < 8) && (i < size); ++i )
				{
				text[i] = b[i];
				if	( ( text[i] < ' ' ) || ( text[i] > '~' ) )
					text[i] = ' ';
				}
			if	( i >= size )
				i = size-1;
			text[i] = 0;
			return i+1;
			}
		else	return snprintf( text, size, "no data" );
		break;
	case 8 :
		if	( iline < ramstock[iram].w32.size() )
			{
			unsigned char * bytes = (unsigned char *)&(ramstock[iram].w32[iline]);
			fmt = "%02X %02X %02X %02X";
			return snprintf( text, size, fmt, bytes[0], bytes[1], bytes[2], bytes[3] );
			}
		else	return snprintf( text, size, "no data" );
		break;
	case 16 :
		if	( iline < ramstock[iram].w32.size() )
			{
			unsigned short * shorts = (unsigned short *)&(ramstock[iram].w32[iline]);
			fmt = "%04X %04X";
			return snprintf( text, size, fmt, shorts[0], shorts[1] );
			}
		else	return snprintf( text, size, "no data" );
		break;
	case 32 :
	default:
		if	( iline < ramstock[iram].w32.size() )
			{
			fmt = "%08X";
			return snprintf( text, size, fmt, ramstock[iram].w32[iline] );
			}
		else	return snprintf( text, size, "no data" );
		break;
	}
}

void target::dump_listing( unsigned int ilist )	// unsecure !!!
{
unsigned int i, ifil, ilin;
int ref;
asmline * daline;
printf("listing %u\n", ilist );
for	( i = 0; i < liststock[ilist].lines.size(); ++i )
	{
	ref = liststock[ilist].lines[i];
	if	( ref < 0 )
		{		// ligne de code source
		ifil = listing::decode_file_index(ref);
		ilin = listing::decode_line_number(ref);
		printf("%-8d %s\n", ilin, filestock[ifil].relpath.c_str() );
		}
	else	{		// ligne asm
		daline = &(asmstock[(unsigned int)ref]);
		// daline->dump();	// ce dump envoie aussi un resume des lignes de src
		printf("%08X %u %s\n", (unsigned int)daline->adr, daline->qbytes, daline->asmsrc.c_str() );
		}
	}
}

// chercher un index dans le listing (-1 si echec)
// on cherche soit :
//	une ligne asm de asmstock dont on a eventuellement trouve l'index dans asmmap
//	une ligne de code source qu'on a trouve dans un fichier source
// recherche par brute-force car on ne peut pas emettre d'hypothese sur l'ordre des index
int listing::search_line( int index, unsigned int hint )
{
unsigned int i;
if	( hint >= lines.size() )
	hint = 0;
for	( i = hint; i < lines.size(); ++i )
	{
	if	( lines[i] == index )
		return (int)i;
	}
for	( i = 0; i < hint; ++i )
	{
	if	( lines[i] == index )
		return (int)i;
	}
return -1;
}

// n'appeler cette methode que si status == 0
void srcfile::readfile()
{
FILE * fil;
fil = fopen( relpath.c_str(), "r" );
if	( fil == NULL )
	{
	fil = fopen( abspath.c_str(), "r" );
	if	( fil == NULL )
		status = -1;
	}
if	( fil )
	{
	char lbuf[256]; int pos;
	while	( fgets( lbuf, sizeof( lbuf ), fil ) )	// aucune confiance dans ce fgets
		{
		lbuf[sizeof(lbuf)-1] = 0;		// il peut omettre le terminateur
		pos = (int)strlen(lbuf) - 1;
		while	( ( pos >= 0 ) && ( lbuf[pos] <= ' ' ) ) // ou rendre une chaine vide
			lbuf[pos--] = 0;			 // on enleve line end et trailing blank
		lines.push_back( string( lbuf ) );
		}
	fclose( fil );
	status = 1;
	}
}

static const signed char hex_digit[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

// translate raw hex bytes into bin 32-bit words
void memory::txt2w32( const char * txt )
{
unsigned int v; int d;
do	{
	v = 0;
	d = hex_digit[(unsigned int)(*(txt++))]; if ( d < 0 ) break; v |= ( d << 4 );
	d = hex_digit[(unsigned int)(*(txt++))]; if ( d < 0 ) break; v |= ( d );
	d = hex_digit[(unsigned int)(*(txt++))]; if ( d < 0 ) break; v |= ( d << 12 );
	d = hex_digit[(unsigned int)(*(txt++))]; if ( d < 0 ) break; v |= ( d << 8 );
	d = hex_digit[(unsigned int)(*(txt++))]; if ( d < 0 ) break; v |= ( d << 20 );
	d = hex_digit[(unsigned int)(*(txt++))]; if ( d < 0 ) break; v |= ( d << 16 );
	d = hex_digit[(unsigned int)(*(txt++))]; if ( d < 0 ) break; v |= ( d << 28 );
	d = hex_digit[(unsigned int)(*(txt++))]; if ( d < 0 ) break; v |= ( d << 24 );
	w32.push_back( v );
	} while ( d >= 0 );
}

// parsing du code executable
void asmline::parse_the_bytes( const char * txt )
{
unsigned int ib = 0, c; int d;
while	( ib < MAXOPBYTES )
	{
	c = *(txt++);
	if	( c == ' ' )	// ignorer le blanc devant un byte, il ne sert a rien
		c = *(txt++);
	if	( c == 0 )
		break;
	d = hex_digit[c] << 4;
	c = *(txt++);
	if	( c == 0 )
		break;
	d |= hex_digit[c];
	if	( d >= 0 )
                bytes[ib++] = d;
	}
qbytes = ib;
}
