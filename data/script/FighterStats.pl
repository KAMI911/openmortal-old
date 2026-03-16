use strict;
use bytes;

=comment

FIGHTER STATS ARE:

ID			int		All fighters have an integer ID...
NAME		string	Name of the fighter displayed to the user
GENDER		int		1=male 2=female
VERSION		int		Version of the fighter's code
DATAVERSION	int		Version of the data file
DATAFILE	string	Filename of the .DAT file.
STARTCODE	sub		Executed when Reset() is called on the fighter

TEAM		string
STYLE		string
AGE			string
WEIGHT		string
HEIGHT		string
SHOE		string
STORY		string

=cut



=comment
We store both official and 3rd-party characters in the %FighterStats hash.
Official characters have ID's between 1 and 99, contributed characters have
ID's >= 100.

No file should access the %FighterStats hash directly, instead,
RegisterFighter must be used to add fighters and
GetFighterStats must be used to retrieve information.
=cut

%::FighterStats = (		

'1'=>
{	'ID'	=> 1,
	'CODENAME' => 'Ulmar',
	'NAME'	=>'Watasiwa baka janajo',
	'TEAM'	=>'Evil',
	'STYLE'	=>'Clown-fu',
	'AGE'	=>'15',
	'WEIGHT'=>'50kg',
	'HEIGHT'=>'168cm',
	'SHOE'	=>'51',
	'STORY'	=>
'After Wastasiwa baka janajo took possession of his time\'s most advanced 
technogadget (read: by accident he was punched so hard that he flew fight into
a high-tech lab, and got tangled with the WrISt(tm)), 

he used all his knowledge to travel to the past (read: he started mashing the buttons, and
this is how it ended). 

Then he knew immediately that he had to destroy
Saturday! (Read: He has absolutely no idea where he is, or what he is doing...)',
	'KEYS'	=>
'Back HPunch - Spinning headbutt
Down Back LPunch - WrISt shot
Forward Back Forward LPunch - WrISt mash',

	'TEAM-hu'	=>'Gonosz',
	'STYLE-hu'	=>'Boh�c-fu',
	'WEIGHT-hu'	=>'50kg + vas�gy',
	'HEIGHT-hu'	=>'168cm',
	'SHOE-hu'	=>'51',
	'STORY-hu'	=>
'Miut�n Wasaiwa baka janaijo elszajr�zta a kora legfejletebb tudom�nyos cuccos�t
(v�letlen�l �gy beh�ztak neki, hogy berep�lt a laboratoriumba �s r�tekeredett a CsUKlo(tm)),

azut�n minden tud�s�t latba vetve vissza utazott a m�ltba (elkezdte nyomkodni a gombokat a
CsUKlo(tm)-en �s ez lett bel�le).

Ezekut�n m�r r�gt�n tudta, hogy itt el kell pusztitania a Szombatot! (� sem tudja, hogy hol
is van �ppen illetve mit is csin�l...)',
	},





'2'=>
{	'ID'	=> 2,
	'CODENAME' => 'UPi',
	'NAME'	=>'Dark Black Evil Mage',
	'TEAM'	=>'Evil leader',
	'STYLE'	=>'Piro-fu',
	'AGE'	=>'30',
	'WEIGHT'=>'70kg',
	'HEIGHT'=>'180cm',
	'SHOE'	=>'42',
	'STORY'	=>
'Member of the Evil Killer Black Antipathic Dim (witted) Fire Mages Leage.
He was sent to destroy Saturday now and forever! Or maybe he has a secret
agenda that noone knows about..? Nah...', 
	'KEYS'	=>
'Back HKick - Spinkick
Forward Forward HKick - Crane kick
Down Back LPunch - Fireball
(also works while crouching)
Back Up HPunch - Burning Hands',

	'NAME-hu'	=>'S�t�t Fekete Gonosz M�gus',
	'TEAM-hu'	=>'Gonosz vez�r',
	'STYLE-hu'	=>'Piro-fu',
	'AGE-hu'	=>'30',
	'WEIGHT-hu'	=>'70kg',
	'HEIGHT-hu'	=>'180cm',
	'SHOE-hu'	=>'42',
	'STORY-hu'	=>
'A Gonosz Gyilkos Fekete Ellenszenves S�t�t (elm�j�) t�zvar�zsl�k lig�j�nak
tagja, kit azzal b�ztak meg, hogy elpuszt�tsa a szombatot egyszer, s
mind�r�kre. Tal�n van valami h�ts� sz�nd�ka, amir�l senki sem tud? Nincs!',
},




'3'=>
{	'ID'	=> 3,
	'CODENAME' => 'Zoli',
	'NAME'	=>'Boxer',
	'TEAM'	=>'Evil',
	'STYLE'	=>'Kickbox-fu',
	'AGE'	=>'16',
	'WEIGHT'=>'80kg',
	'HEIGHT'=>'180cm',
	'SHOE'	=>'43',
	'STORY'	=>
'Boxer joined the Mortal Szombat team for the sole purpose to punch as
many people as hard as he possibly can. He has no other purpose
whatsoever, but at least this keeps him entertained for the time being.', 
	'KEYS'	=>
'Back HPunch - Spinning punch
Down Back LPunch - Weight toss
Forward Forward HPunch - Leaping punch',

	'NAME-hu'	=>'Boxer',
	'TEAM-hu'	=>'Gonosz',
	'STYLE-hu'	=>'Kickbox-fu',
	'AGE-hu'	=>'16',
	'WEIGHT-hu'	=>'80kg',
	'HEIGHT-hu'	=>'180cm',
	'SHOE-hu'	=>'43',
	'STORY-hu'	=>
'Boxer az�rt csatlakozott a Mort�l Szombat csapathoz, hogy min�l t�bb
embernek besomhasson, min�l t�bbsz�r, �s min�l nagyobbat. M�s c�lja ezen
k�v�l nincs, de ez is remek�l elsz�rakoztatja k�z�pt�von',
},




'4'=>
{	'ID'	=> 4,
	'CODENAME' => 'Cumi',
	'NAME'	=>'Cumi',
	'TEAM'	=>'Good Leader',
	'STYLE'	=>'N/A',
	'AGE'	=>'15',
	'WEIGHT'=>'55kg',
	'HEIGHT'=>'170cm',
	'SHOE'	=>'41.5',
	'STORY'	=>
'His life ambition was to drive a car. Now that this was accomplished,
he has turned to his second greatest ambition: to be a great martial
artist superhero. As a start, he has watched the TV series "Kung fu"
from beginning to end, in one session. His current training consists
of but this.',
	'KEYS'	=>
'Down Back LPunch - Finger Shot
Forward Forward HPunch - Spit
Back Down Forward - Baseball',

	'NAME-hu'  =>'Cumi',
	'TEAM-hu'  =>'Jo vezer',
	'STYLE-hu' =>'N/A',
	'AGE-hu'   =>'15',
	'WEIGHT-hu'=>'55',
	'HEIGHT-hu'=>'170',
	'SHOE-hu'  =>'41.5',
	'STORY-hu' =>
'Elete fo ambicioja volt, hogy autot vezessen. Most hogy ezt teljesitette, masodik fo
amibicioja fele fordult: hogy nagy harcmuvessze valjon. Kezdetben ehhez megnezte
a Kung fu sorozatot elejetol vegeig egyulteben. Kepzettsege jelenleg ebbol all.',
},




'5'=>
{	'ID'	=> 5,
	'CODENAME' => 'Sirpi',
	'NAME'	=>'Sirpi',
	'TEAM'	=>'Good',
	'STYLE'	=>'Don\'tHurtMe-FU',
	'AGE'	=>'24',
	'WEIGHT'=>'76kg',
	'HEIGHT'=>'170cm',
	'SHOE'	=>'41',
	'STORY'	=>
'After being a "hardcore" gamer for several years and consuming a
great amount of food, his electricity was turned off. This has caused
him to make a very stupid face which lasts till this day, and will
last until he has defeated his archenemy. This is why he resolved to
join the good team... also he is frightened alone.',
	'KEYS'	=>
'Down Forward LPunch - Surprise
Forward Forward HPunch - Applause',


	'STYLE-hu'	=> 'Neb�nts-FU',
	'STORY-hu'	=>
'Sok �vnyi hardcore gamerked�s ut�n, mik�zben el is h�zott j�l,
kikapcsolt�k n�la a villanyt.

Erre nagyon h�lye pof�t v�gott, �s ez igy is
marad mindaddig, am�g le nem sz�mol �sellens�g�vel (vagy m�g ut�na is).
Ez�rt csatlakozott a j�k kicsiny csapat�hoz... Meg am�gy is f�l egyed�l.',
},





'6'=>
{	'ID'	=> 6,
	'CODENAME' => 'Macy',
	'NAME'	=>'Macy',
	'TEAM'	=>'Good',
	'STYLE'	=>'Macy-fu',
	'AGE'	=>'17',
	'WEIGHT'=>'41kg',
	'HEIGHT'=>'175cm',
	'SHOE'	=>'37',
	'STORY'	=>
'A few years ago (perhaps a little earlier, or maybe later) she was
found among the clouds in a cradle (falling, of course). She learned
martial art from brave Son Goku, so she landed on her feet and didn\'t
die. She\'s been immortal ever since. Who knows for how long? Maybe
it won\'t be until the next fight agains Evil...',
	'KEYS'	=>
'Down Back LPunch - Toss
Forward Forward HKick - Scissor Kick',

	'STORY-hu'	=>
'kb. n�h�ny �vvel ezel�tt, (vagy tal�n egy kicsit kor�bban, esetleg
k�s�bb) a felh�k k�z�tt egy p�ly�ban tal�ltak r� (zuhan�s k�zben).

A b�tor
Songokut�l elleste a harcm�v�szet mesteri fort�lyait, igy talpra esett �s
nem halt meg. Az�ta is halhatatlan. Ki tudja, m�g meddig? Tal�n a
k�vetkez� harcig a gonosz ellen...',
	},




'7'=>
{	'ID'	=> 7,
	'CODENAME' => 'Bence',
	'NAME'	=>'Jan Ito',
	'TEAM'	=>'Evil',
	'STYLE'	=>'Kururin-do',
	'AGE'	=>'20',
	'WEIGHT'=>'85kg',
	'HEIGHT'=>'172cm',
	'SHOE'	=>'39',
	'STORY'	=>
'The "Japanese giant" is a sworn enemy of Descant... after he left
muddy boot marks all over the freshly mopped porch of the pub, er,
dojo which has belonged to his ancestors for 16 generations. Since
he has turned to the dark side of the janitor. His knowledge of
the "way of the concierge" matches his deep hatred towards army
boots.',
	'KEYS'	=>
'Down Back LPunch - Soap Throw
Back Fw Back Fw LPunch - Stick Spin
Back Forward HPunc - Pierce',

	'NAME-hu'	=>'Taka Ito',
	'TEAM-hu'	=>'Gonosz',
	'STYLE-hu'	=>'Kururin-do',
	'AGE-hu'	=>'20',
	'WEIGHT-hu' =>'85',
	'HEIGHT-hu' =>'172',
	'SHOE-hu'	=>'39',
	'STORY-hu'	=>
'A jap�n �ri�s Descant esk�dt ellens�ge, mi�ta �sszej�rta az �ltala frissen felmosott
verand�t a 16 gener�ci� �ta csal�dja �ltal birtokolt foga-do-ban. Tud�s�t a s�t�t
oldal szolg�lat�ba �ll�totta. Tud�s�t a "gondnok �tj�n" csak m�lys�ges megvet�se a
vasaltorr� bakancsok ir�nt sz�rnyalja t�l.',
	},




'8'=>
{	'ID'	=> 8,
	'CODENAME' => 'Grizli',
	'NAME'	=>'Grizzly',
	'TEAM'	=>'Good',
	'STYLE'	=>'Bear dance',
	'AGE'	=>'21',
	'WEIGHT'=>'Several tons',
	'HEIGHT'=>'170cm',
	'SHOE'	=>'49',
	'STORY'	=>
'Grizzly has been long famous for his laziness. He has made laziness a
form of art. In the past 5 years he has been to lazy to watch TV. Every
Saturday he trains in his own special fighting style, one not unlike
that of Bud Spencer, whom he holds as his honorary master. Though,
since he found out that Bud WORKS on Saturdays, he has revoked his
title, and keeps it for himself. He has joined the Good team to fight
to protect the Saturday.',
	'KEYS'	=>
'Down Back LPunch - Bear Shot
Forward Forward HPunch - Poke
Down Down LKick - Earthquake
Back Forward Back HPunch - Nunchaku',

	'NAME-hu'	=>'Grizli',
	'TEAM-hu'	=>'J�',
	'STYLE-hu'	=>'Gyak�s ala Medve',
	'AGE-hu'	=>'21',
	'WEIGHT-hu'	=>'50000000',
	'HEIGHT-hu'	=>'170',
	'SHOE-hu'	=>'49',
	'STORY-hu'	=>
'Grili a lustas�g�r�l volt hires mindig. Olyannyira, hogy amilyen szinten
azt csin�lja, az m�r m�v�szet. Az ut�bbi 5 �vben m�r a TV
n�z�shez is lusta lett.

Minden Szobaton tart edz�st a K�l�nbenmegintd�hbej�v�nk do
stilusb�l, amit m�g kezd� kor�ban a TV-b�l saj�t�tott el. A stilus
tiszteletbeli nagymestere maga B�d Szpencer, de sajnos miut�n B�d-r�l
kider�lt, hogy szombatonk�nt dolgozik, Grizli elvette t�le a cimet, s
az�ta mag�nak tartogatja.

Grizli a szombat ellenesek �d�z gy�l�l�je, a j� csapat oszlopos tagja.',
	},




'9'=>
{	'ID'	=> 9,
	'CODENAME' => 'Descant',
	'NAME'	=>'Descant',
	'TEAM'	=>'Good',
	'STYLE'	=>'Murderization',
	'AGE'	=>'58',
	'WEIGHT'=>'89kg',
	'HEIGHT'=>'180cm',
	'SHOE'	=>'44',
	'STORY'	=>
'He was trained in \'Nam in every known weapon and martial art form.
He fought there on the side of the Americans and the Russians...
whoever paid more at the moment. Then he used the money to hybernate
himself until the next great war.. or until the Saturday is in
trouble. He joined the side with the more CASH...',
	'KEYS'	=>
'Down Back LPunch - Aimed Shot
Back Back LPunch - Hip Shot
Forward Down HPunch - Knife Throw
Forward Forward HPunch - Gun Hit',

	'NAME-hu'	=>'Descant',
	'TEAM-hu'	=>'J�',
	'STYLE-hu'	=>'+halol',
	'AGE-hu'	=>'58',
	'WEIGHT-hu'=>'89',
	'HEIGHT-hu'=>'180',
	'SHOE-hu'	=>'44',
	'STORY-hu'	=>
'A Vietn�mi h�bor� sor�n k�pezt�k ki minden ismert fegyverre �s harcm~uv�szetre. M�r ott
is az Oroszok �s az Amerikaik oldal�n harcolt, m�r aki �ppen t�bbet fizetett. Ezut�n a p�nzb"ol
hibern�ltatta mag�t �s csak h�bor�k eset�n olvasztatja f�l mag�t, vagy most mikor a szombat
bajba ker�l most is azon az oldalon van, ahol vastagabb a BUKSZA, most �pp a...',
},





'10'=>
{	'ID'	=> 10,
	'CODENAME' => 'Surba',
	'NAME'	=>'Rising-san',
	'TEAM'	=>'Evil',
	'STYLE'	=>'Flick-fu',
	'AGE'	=>'500',
	'WEIGHT'=>'N/A',
	'HEIGHT'=>'50',
	'SHOE'	=>'N/A',
	'STORY'	=>
'Mistically disappeared Aeons ago.. on a Saturday! Now he is back, and
brought back his destructive techique, unmatched on Earth. Noone knows
why he joined the Dark Evil Mage...',

	'NAME-hu'	=>'Rising-san',
	'TEAM-hu'	=>'Gonosz',
	'STYLE-hu'	=>'P�cc-fu',
	'AGE-hu'	=>'500',
	'WEIGHT-hu'	=>'N/A',
	'HEIGHT-hu'	=>'50',
	'SHOE-hu'	=>'Nem visel',
	'STORY-hu'	=>
'Sok-sok �vvel ezel"ott elt�nt misztikus k�r�lm�nyek k�z�tt... egy szombati napon!
�s most visszat�rt. Senki sem tudja honnan j�tt, de mag�val hozta puszt�t� technik�j�t
melynek nincs p�rja a f�ld�n. Senki sem �rti mi�rt fogadta el a gonosz var�zsl
megb�z�s�t...',
},





'11'=>
{	'ID'	=> 11,
	'CODENAME' => 'Ambrus',
	'NAME'	=>'Mad Sawman',
	'TEAM'	=>'Evil',
	'STYLE'	=>'Sawing',
	'AGE'	=>'35',
	'WEIGHT'=>'110',
	'HEIGHT'=>'120',
	'SHOE'	=>'49',
	'STORY'	=>
'His cradle was found on a tree. Later he chopped up the family that
took him and fed them to the bears. He has been roaming the Canadian
forests, chopping trees and heads alike. On hot summer nights his
maniac laughter echoes far.',
	'KEYS' =>
'Down Back LPunch - Axe Toss
Back Forward HKick - Chop Chop
Forward Forward LKick - Bonesaw',

	'NAME-hu'	=>'F�r�szes �r�lt',
	'TEAM-hu'	=>'Gonosz',
	'STYLE-hu'	=>'Fany�v�',
	'AGE-hu'	=>'35',
	'WEIGHT-hu'	=>'110',
	'HEIGHT-hu'	=>'120',
	'SHOE-hu'	=>'49',
	'STORY-hu'	=>
'B�lcs�j�t egy f�n tal�lt�k meg. K�s�bb felapr�totta az eg�sz befogad� csal�dj�t, �s
megetette a medv�kkel. Az�ta a kanadai erd�kben bolyongva v�gja a f�kat �s az
emberfejeket. Forr� ny�ri �jszak�kon mindig hallatszik �r�lt kacaja.',
},




'12'=>
{	'ID'	=> 12,
	'CODENAME' => 'Dani',
	'NAME'	=>'Imperfect Soldier',
	'TEAM'	=>'Good',
	'STYLE'	=>'Pub Fight',
	'AGE'	=>'50',
	'WEIGHT'=>'100',
	'HEIGHT'=>'180',
	'SHOE'	=>'44',
	'STORY'	=>
'His childhood was determined by Drezda\'s bombing. This trauma has
caused him to join the army. For the last 30 years he is corporal
without the slightest hope for advancement. He annoys his
subordinates with a constant flow of stories of pub fights, until
they ask for relocation.',
	'KEYS' =>
'Down Back LPunch - Hat
Forward Forward HPunch - Ramming Attack
Back Down Back LPunch - Stab
Back Forward LKick - Poke',

	'NAME-hu'	=>'T�k�letlen Katona',
	'TEAM-hu'	=>'J�',
	'STYLE-hu'	=>'Kocsmabuny�',
	'AGE-hu'	=>'50',
	'WEIGHT-hu'	=>'100',
	'HEIGHT-hu'	=>'180',
	'SHOE-hu'	=>'44',
	'STORY-hu'	=>
'Gyermekkor�t meghat�rozta Drezda lebomb�z�sa. E trauma hat�s�ra katonai
p�ly�ra �llt. Imm�ron 30 �ve a Bundeswehr k�tel�k�ben tizedes az
el�l�ptet�s b�rminem~u es�lye n�lk�l.

Alantasait folytonosan kocsmai buny�inak t�rt�neteivel trakt�lja, am�g azok
�thelyez�s�ket nem k�rik.',
},





'13'=>
{	'ID'	=> 13,
	'CODENAME' => 'Kinga',
	'NAME'	=>'Aisha',
	'TEAM'	=>'Good',
	'STYLE'	=>'Death Dance',
	'AGE'	=>'21',
	'WEIGHT'=>'43.5',
	'HEIGHT'=>'155',
	'SHOE'	=>'35',
	'STORY'	=>
'Her trials started right in the womb.. her life hung on a single
umbilical cord! But she was finally born, and got the name
Aisha ("survives everything"). Since her childhood she survived
natural disasters and terrorist attacks, and got frankly fed up.
So one time she said:

"If I survive this, I swear, I\'ll join those stupid Mortal guys!"',

	'STORY-hu'=>
'A megprobáltatások akkor kezdődtek, amikor anyukája a szíve alatt hordta.
Egyetlen köldökzsinoron függött az élete! De megszületett végül, ezért
kapta az Aisha ("mindent túlélő") nevet. Aztan gyermekkorától fogva sok
természeti katasztrófat, terrortámadást átveszélt, és mar kezdett elege
lenni az egészből, igy hát az egyik alkalommal kijelentette, hogy ha ezt
túlelem, csatlakozom azokhoz a hülye Mortálosokhoz!',
},	#'



'15'=>
{	'ID'	=> 15,
	'CODENAME' => 'Elf',
	'NAME'	=> 'Pixie',
	'TEAM'	=> 'Good',
	'STYLE'	=> 'Glamour',
	'AGE'	=> '140',
	'WEIGHT'=> '1',
	'HEIGHT'=> '1',
	'SHOE'	=> '1',
	'STORY'	=> '...',
},



'16'=>
{	'ID'	=> 16,
	'CODENAME' => 'Judy',
	'NAME'	=> 'Judy',
	'TEAM'	=> 'Evil',
	'STYLE'	=> '?',
	'AGE'	=> '?',
	'WEIGHT'=> '?',
	'HEIGHT'=> '?',
	'SHOE'	=> '?',
	'STORY'	=> '...',
},



'14'=>
{	'ID'	=> 14,
	'CODENAME' => 'Misi',
	'NAME'	=>'Papatsuka Mamatsuba',
	'TEAM'	=>'Evil',
	'STYLE'	=>'Gloom',
	'AGE'	=>'Feudal Middle',
	'WEIGHT'=>'Dead',
	'HEIGHT'=>'178 cm',
	'SHOE'	=>'43,12748252',
	'STORY'	=>
'Papastuka has been raised strictly in the way of the samurai since age 4.
His father was the most famous warrior of the past 20 years. After he
learned all the jutsu from dad, he skalped him, and put the skalp on his head
to scare his enemies.

On weekdays he is seen chasing women, saturdays he
drinks a lot. Then he decided, that enjoying saturday should belong to him
alone...',

	'NAME-hu'	=>'Apatsuka Anyatsuba',
	'TEAM-hu'	=>'Gonosz',
	'STYLE-hu'	=>'Komor',
	'AGE-hu'	=>'Feud�lis k�z�p...',
	'WEIGHT-hu'	=>'Nagyon s�lyos!',
	'STORY-hu'	=>
'Apatsuk�t 4 �ves kora �ta nevelt�k sz�lei a szamur�j �letm�dra, szigor
keretek k�z�tt. Apja az elm�lt 20 �v legh�resebb harcosa volt. Amint
minden harci fog�st elsaj�t�tott apj�t�l, megskalpolta �s a skalpj�t
fej�re illesztette, ezzel megf�leml�tve ellens�geit.

H�tk�zben n�ket
hajszolt, szombaton sz�vott �s berugott. Azt�n �gy d�nt�tt, hogy ezen a
szombaton csak � �rezheti j�l mag�t...',


},


);



sub RegisterFighter($)
{
	my ($reginfo) = @_;
	
	# reginfo must contain: ID, GENDER, DATAVERSION, DATASIZE, STARTCODE, FRAMES, STATES, CODENAME
	foreach my $attr (qw(ID GENDER DATAVERSION DATASIZE STARTCODE FRAMES STATES CODENAME))
	{
		die "RegisterFighter: Attribute $attr not found" unless defined $reginfo->{$attr};
	}
	# CheckStates( $reginfo->{ID}, $reginfo->{STATES} );
	
	my ($fighterenum, $fighterstats);
	$fighterenum = $reginfo->{ID};
	
	$fighterstats = $::FighterStats{$fighterenum};
	if ( not defined $fighterstats )
	{
		print "RegisterFighter: Fighter $fighterenum not found, non-syndicated?\n";
		$fighterstats = {
			'ID'		=> $fighterenum,
			'NAME'		=>'Unknown (non-syndicated)',
			'TEAM'		=>'Unknown',
			'STYLE'		=>'Unknown',
			'AGE'		=>'Unknown',
			'WEIGHT'	=>'Unknown',
		  	'HEIGHT'	=>'Unknown',
			'SHOE'		=>'Unknown',
			'STORY'		=>'...',
			'DATAFILE'	=> $reginfo->{CODENAME} . '.dat',
			%{$reginfo}
  		};
		$::FighterStats{$fighterenum} = $fighterstats;
		return;
	}
	
	# Add the reginfo to the fighter stats:
	%{$fighterstats} = ( 'DATAFILE' => $reginfo->{CODENAME}.'.dat', %{$fighterstats}, %{$reginfo} );
}



sub GetStatsTranslated($$)
{
	my ($source, $stat) = @_;
	my $val = $source->{"${stat}-$::LanguageCode"};
	$val = $source->{$stat} unless defined $val;
	# The -hu strings are stored as ISO-8859-2.  Four Hungarian characters
	# differ from their Unicode (Latin-1) counterparts and must be re-encoded
	# as proper UTF-8 so that DrawTextMSZ (sge_tt_textout_UTF8) renders them
	# correctly: \xD5=Ő \xF5=ő  \xDB=Ű \xFB=ű
	if ( defined $val && defined $::LanguageCode && $::LanguageCode eq 'hu' ) {
		$val =~ s/\xD5/\xC5\x90/g;  # Ő -> UTF-8
		$val =~ s/\xF5/\xC5\x91/g;  # ő -> UTF-8
		$val =~ s/\xDB/\xC5\xB0/g;  # Ű -> UTF-8
		$val =~ s/\xFB/\xC5\xB1/g;  # ű -> UTF-8
	}
	return $val;
}



sub GetFighterStats($)
{
	my ($fighterenum) = @_;
	
	my ($source) = $::FighterStats{$fighterenum};

	$::Codename	= $source->{CODENAME};
	$::Name		= GetStatsTranslated( $source, 'NAME' );
	$::Team		= GetStatsTranslated( $source, 'TEAM' );
	$::Style	= GetStatsTranslated( $source, 'STYLE' );
	$::Age		= GetStatsTranslated( $source, 'AGE' );
	$::Weight	= GetStatsTranslated( $source, 'WEIGHT' );
	$::Height	= GetStatsTranslated( $source, 'HEIGHT' );
	$::Shoe		= GetStatsTranslated( $source, 'SHOE' );
	$::Story	= GetStatsTranslated( $source, 'STORY' );
	$::Keys		= GetStatsTranslated( $source, 'KEYS' );
	$::Datafile	= $source->{'DATAFILE'};
	$::Portrait	= $::Codename . ".icon.png" if defined $::Codename;
	
	$::Story =~ s/([^\n])\n([^\n])/$1 $2/gms if defined $::Story;
	
	if ( defined $::LanguageCode && $::LanguageCode eq 'hu' ) {
		@::StatTags = ( "N\xC3\xA9v: ", 'Csapat: ', "St\xC3\xADlus: ", 'Kor: ', "S\xC3\xBAly: ", "Magass\xC3\xA1g: ", "Cip\xC5\x91m\xC3\xA9ret: " );
	} else {
		@::StatTags = ( 'Name: ', 'Team: ', 'Style: ', 'Age: ', 'Weight: ', 'Height: ', 'Shoe size: ' );
	}
	
	# print "The data file of $fighterenum is '$::Datafile'\n";
	
	return $source;
}


sub GetNumberOfAvailableFighters()
{
	my ($id, $fighter, $i);
	
	$i = 0;
	while ( ($id, $fighter) = each %::FighterStats )
	{
		++$i if defined( $fighter->{DATAFILE} );
	}
	$::CppNumberOfAvailableFighters = $i;
}



=comment

Returns the number of .pl files in the fighters directory.

param $CharactersDir	The name of the characters directory. Might be stored and used later on.

=cut

sub GetNumberOfFighterFiles($)
{
	($::CharactersDir) = shift;
	# Loads the list of .pl files in the characters directory.

	my (@files, $file );

	opendir CHARDIR, $::CharactersDir;
	@files = readdir CHARDIR;
	closedir CHARDIR;

	foreach $file (@files) {
		push @::CharacterList, $file if ( $file =~ /.pl$/ );
	}

	return scalar @::CharacterList;
}


=comment
Loads a fighter file from the characters directory.
=cut

sub LoadFighterFile($)
{
	my ($index) = @_;

	if ($index < 0 or $index >= scalar @::CharacterList) {
		print "LoadFighterFile: Couldn't load index $index\n";
	}

	my ($filename, $return);
	$filename = $::CharactersDir . '/' . $::CharacterList[$index];
	
	unless ( $return = do $filename ) {
		print "Couldn't parse $filename: $@\n"	if $@;
		print "Couldn't do $filename: $!\n"		unless defined $return;
		print "Couldn't run $filename\n"		unless $return;
	}
}


return 1;
