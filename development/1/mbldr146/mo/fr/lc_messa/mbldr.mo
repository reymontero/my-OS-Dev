??    s      ?  ?   L  ?   ?	  ?   ?
     d  @   |  ?  ?  $   ?  5   ?  .   ?       (   0     Y  *   g  B   ?  @   ?  7        N     k     {  *   ?     ?     ?  *   ?       -   =     k  ?   q  ?   ?     3  )   Q  )   {  4   ?     ?  -   ?  %        @     \     r     ?  K   ?     ?  !     +   *  B   V  O   ?     ?     ?  "       6     R     `     x  0   ?  L   ?           .  <   L     ?  5   ?  ]   ?     :     T     f     w  A   ?     ?  %   ?          "     2  %   P  2   v  /   ?  -   ?       C        X  L   a  ?   ?  ?   >  $   ?  6        O  '   e  0   ?  #   ?     ?         .      /   N   /   ~       ?   2   ?   (   !  (   +!  ;   T!  :   ?!  ;   ?!  -   "  &   5"  ;   \"  :   ?"  ;   ?"     #  &   &#  %   M#     s#     ?#     ?#     ?#  !   ?#     ?#  "   $  E   2$  
   x$  "   ?$    ?$  ?   ?%     ?&  4   ?&  ?  ?&  I   ?,  5   ?,  .   &-     U-  )   n-     ?-  8   ?-  Y   ?-  b   ?.  Q   ?.  "   ?.     /  .   -/  .   \/     ?/      ?/  C   ?/  !   ?/  (   0     H0  h   M0  ?   ?0     71  .   U1  )   ?1  ;   ?1     ?1  *   2  "   22     U2     n2     ?2     ?2  c   ?2     3  #   43  4   X3  d   ?3  D   ?3     74  %   T4  $   z4  ?  ?4     u7     ?7     ?7  /   ?7  _   ?7  -   C8  %   q8  W   ?8  !   ?8  @   9  ?   R9     ?9     ?9     :     :  H   ?:     ?:  $   ?:     ?:     ?:  +   ?:  .   ;  8   M;  6   ?;  =   ?;     ?;  S   
<  	   ^<  P   h<  ?   ?<  ?   U=  '   D>  <   l>     ?>  0   ?>  <   ?>  )   6?  .   `?  *   ??  9   ??  9   ??  =   .@  )   l@  :   ?@  .   ?@  5    A  I   6A  H   ?A  L   ?A  9   B  =   PB  H   ?B  G   ?B  K   C     kC  0   ?C  +   ?C     ?C     D     "D     5D  <   MD     ?D  "   ?D  N   ?D     E  $   !E  q   
           l          )   o   9   j   J   #      +              g   S   [       n                 @   I   e       /   p                     a   0      O      >              M   3   H               s       m   b   *       ,          E   !   X       d      %   A       h   ;   k   V           L       1   C          ]   \       `      2           $   G      Q   U          D           5           :      =         N   ^   4   (       	      7           ?       F   R   c   8   i   .   <      _   6   K      r   '      &         T   -   "   Z   f   B   P                                Y   W       -d <device> Defines a name of device (usually hard disk) to manage.
    This is typically /dev/hda or /dev/sda under Linux and /dev/ad0 or
    /dev/da0 under BSD. It also can be a regular file representing binary
    image of some device.  -d <device_number> Defines BIOS device number if hard disk autodetect does not
    work. Parameter should be in the following form: '0x80', '0x81', etc.
    (without apostrophes) '%c' option is detected 'Enhanced Disk Drive (EDD) Support' extensions subset is present 'Next HDD' menu item adds the following capability to the
boot loader: after choosing this menu item mbldr tries to
load MBR from another hard drive (number of drive is increased
sequentially: 0x80, 0x81, etc) and execute it. New master boot
loader gets a number of drive being used as a parameter.
Theoretically this scheme should work fine assuming that all
boot loaders respect this input parameter. Unfortunately
several boot loaders ignore it assuming that the drive number
used to boot is always 0x80 (what is incorrect according to
EDD3 specification). It may lead to impossibility to boot for
so called 'passive' master boot loaders (that just try to boot
not updating MBR every time you boot). But even worse for so
called 'active' master boot loaders (that modify MBR every time
you boot) you may overwrite MBR on the wrong disk. Please
note that 'Next HDD' feature is not the same as the one
provided by BIOS (boot from either of installed hard disks).
BIOS does the reenumeration of hard drives making bootable
disk always 0x80, while mbldr does not. Use this feature at
your own risk! It is dangerous and may lead to data loss if
you have master boot loaders others than mbldr with version
1.37 or above!
Are you sure? 'Next HDD' menu item has been added. AIX boot (PS/2 port) or OS/2 (v1.0-1.3) or SplitDrive AIX data or Coherent filesystem or QNX 1.x-2.x AST SmartSleep Partition All 256 words of data have been received Are you sure? Boot menu body with the list of partitions Boot menu text will be generated after adding bootable partitions. Buffer size does not suit the length of Device Path Information. Buffer size is %u, Device Path Information length is %u Calculating approximate size Choose a device Choose partition or menu item Compaq configuration/diagnostics partition DOS 12-bit FAT DOS 3.0+ 16-bit FAT (up to 32M) Default partition is what was booted last. Device name has been set to %s Either ATA or SATA interface type is detected Empty Error writing log, it seems there is no space left on a device. Existing mbldr was found on the chosen disk device. Would you
like to configure it? (otherwise a fresh install will be performed) Extended partition (DOS 3.3+) Extended partition 0x%02X has been found. Extended partition, LBA-mapped (WIN95/98) Extensions are not present, device will not be used. Found devices: %u Hidden DOS 12-bit FAT or Leading Edge DOS 3.x Hidden DOS 16-bit FAT <32M or AST DOS Hidden DOS 16-bit FAT >=32M Hidden NTFS/HPFS(IFS) Host bus type was: '%s' Interface type was: '%s' Internal error with dynamic list allocation logic, this should never happen Invalid -d parameter: %s Invalid Device name (Device='%s') Invalid prefix in Device name (Device='%s') Key, indicating presence of Device Path Information was not found. Length of string of DeviceName should be equal to 4, but it is %i (Device='%s') List is now empty. Log level set to DEBUG Magic number 0xAA55 was not found. Master Boot LoaDeR is a boot loader for master boot record intended to support
booting from different partitions. It has simple text boot menu and occupies
only the first sector of hard disk. It is intended to replace MBR contents
coming with MS-DOS/Windows. It supports partition hiding, booting OSes above
1024 cylinder, active partition switching and may load default OS by timeout.
It is also capable to boot Linux/BSD partitions. This program is intended to
install and configure mbldr on the HDD you choose using interactive menus.

 Master device Memory allocation error Model number was: '%s' More than one extended partition has been found. No devices has been found, try to explicitly specify device with '-d' option No found matches, errno=%i, (%s) No partitions have been found Number of available characters for custom boot menu text: %i Number of characters left=%i Only one suitable device was found and will be used:  Options are:
 -h Outputs help message and exits
 -v Verbose mode (debug only, uses log file)
 Partition has been added. Primary interface Quit immediately Read error, errno=%i, (%s) Read operation returned unexpected number of bytes being read: %i Reading sector %lu. Running out of memory, errno=%i, (%s) Secondary interface Sector size: %u Should be 0xBEDD, found: 0x%X Size of a sector is not equal to 512. Size of a structure is equal either to 74 or to 66 Size of a structure is greater or equal than 30 Size structure returned by BIOS interrupt: %u Slave device Structure returned by 'get device parameters' is less than minimum. Syntax:  The number of partitions reached its maximum (9). You can't add another one. Too many characters have been entered for labels of bootable partitions.
Installation of mbldr is not allowed until you free at least %i bytes. Too many characters in custom boot menu text.
Regenerate boot menu text automatically or upload smaller file.
Installation of mbldr is not allowed until you free at least %i bytes. Too many partitions have been found. Too small hard drive, it seems some error has occured. Trying disk device %s Trying to get description for device %s Trying to read device model/vendor for device %s Type the label for this partition:  Unable to close device %s, %s Unable to close disk device %s Unable to close temporary file, errno=%i, (%s) Unable to create temporary file, errno=%i, (%s) Unable to delete temporary file, errno=%i, (%s) Unable to execute sysctl command Unable to get maximum number of available devices. Unable to open device %s for reading, %s Unable to open device %s for writing, %s Unable to open file with device description, errno=%i, (%s) Unable to open file with model description, errno=%i, (%s) Unable to open file with vendor description, errno=%i, (%s) Unable to open temporary file, errno=%i, (%s) Unable to read data from device %s, %s Unable to read file with device description, errno=%i, (%s) Unable to read file with model description, errno=%i, (%s) Unable to read file with vendor description, errno=%i, (%s) Unable to read sector. Unable to seek to desired position, %s Unable to write data to device %s, %s Unable to write sector. Unknown error, errno=%i, (%s) Unknown option %c Unknown or not recognized User asked for command-line help. What do you want to do? Windows NT NTFS or OS/2 HPFS (IFS) Write operation returned unexpected number of bytes being written: %i XENIX root glob() system call succeeded on %s  -d <device> Définit le nom de l'unité (habituellement un disque dur) à gérer.
    Typiquement, ce sera /dev/hda ou /dev/sda sous Linux et /dev/ad0 ou /dev/da0 sous BSD. Ce peut être également le nom d'un fichier régulier qui contient l'image binaire d'une unité.  -d <device_number> Définit le numéro BIOS de l'unité, si l'autodétection du disque dur ne fonctionne pas. Le paramètre doit être de la forme suivante: '0x80', '0x81', etc.
    (sans les apostrophes) option '%c' détectée Présence de l'extension 'Enhanced Disk Drive' (EDD) Le choix de menu 'Next HDD' (unité de disque suivante) ajoute au chargeur 
de démarrage la fonctionnalité suivante:

Après avoir sélectionné cette option mbldr essaie de charger le MBR
depuis un autre disque (le numéro est incrémenté séquentiellement:
0x80, 0x81, etc) et l''exécute.  Le nouveau master boot loader prend
le numéro d'unité en utiliser en paramètre.

Théoriquement, ce schéma devrait fonctionner en assumant que tous les
chargeurs de démarrage respectent ce paramètre d'entrée.  Hélas
plusieurs chargeurs ignorent ce dernier, en assumant que le numéro
d'unité à démarrer est toujours 0x80 (ce qui est incorrect selon la
spécification EDD3). Ceci peut mener à l'impossibilité de démarrer
pour les chargeurs dits 'passifs' (qui cherchent seulement à démarrer,
pas à modifier le MBR à chaque démarrage). Mais c'est pire encore pour
les chargeurs de MBR dits 'actifs' (qui modifient le MBR à chaque
démarrage), car vous pouvez écraser le MBR d'un autre disque.  Notez
bien que la fonction 'Next HDD' n'est pas la même que celle fournie
par le BIOS (démarrer depuis l'un ou l'autre des disques durs).
Le BIOS renumérote en effet les disques, attribuant toujours le numéros 0x80
au disque à booter, chose que mbldr ne fait pas.

Utilisez cette fonction à vos risques et périls ! Elle est dangereuse et peut
provoquer des pertes de données si vous utilisez des chargeurs autres que mbldr
version 1.37 ou supérieure.

Êtes-vous certain(e) ? L'option de menu 'Next HDD' (unité de disque suivante) a été ajoutée. AIX boot (PS/2 port) ou OS/2 (v1.0-1.3) ou SplitDrive AIX data ou Coherent filesystem ou QNX 1.x-2.x Partition AST SmartSleep Les 256 mots de données ont été reçus Êtes-vous sûr(e)? Corps du menu de démarrage, avec la liste de partitions Le texte du menu de démarrage sera créé après avoir ajouté les partitions bootables. La taille du tampon ne correspond pas à la longueur de l'information de chemin de périphérique. La taille du tampon est %u,  l'information de chemin de périphérique %u octets. Je calcule la taille approximative Choisissez une unité Choisissez une partition ou un entrée de menu Partition de configuration/disagnostics Compaq FAT-12 (DOS) FAT-16 (DOS 3.0+, jusqu'à 32Mo) La partition par défaut est celle qui a été démarré en dernier Le nom d'unité est maintenant %s Une interface ATA ou SATA est détectée Vide Erreur lors de l'écriture du journal, il semble qu'il ne reste plus assez de place sur le péripherique Un mbldr est déjà présent sur l'unité de disque. Souhaitez-vous le 
configurer ? (sinon une réinstallation sera réalisée) Partition étendue (DOS 3.3+) La partition étendue 0x%02X a été trouvée. Partition étendue, LBA-mapped (WIN95/98) Extensions absentes, le périphérique ne sera pas utilisé Périphériques trouvés: %u DOS FAT-12 cachée ou Leading Edge DOS 3.x DOS FAT-16 cachée <32M ou AST DOS DOS FAT-16 cachée >=32M NTFS/HPFS(IFS) cachée Type de bus hôte : '%s' Type d'interface: '%s' Erreur interne dans la logique d'allocation d'une liste dynamique, ce qui ne devrait jamais arriver Paramètre -d invalide: %s Nom d'unité invalide (Device='%s') Préfixe invalide pour le nom d'unité (Device='%s') La clé indiquant la présence de l'information de chemin de périphérique, n'a pas été trouvée. DeviceName devrait avoir 4 caractères, but %i trouvés(Device='%s') La liste est vide maintenant Niveau de journalisation mis à DEBUG Le nombre magique 0xAA55 est absent. Master Boot LoaDeR est un chargeur de démarrage pour MBR (Master Boot
Record) prévu pour supporter le démarrage depuis différentes
partitions. Il présente un menu de démarrage en texte simple, et
occupe seulement le premier secteur du disque dur. Il est prévu pour
remplacer le contenu du MBR livré avec MS-DOS ou Windows. Il permet de
cacher des partitions, de démarrer des systèmes au-delà du cylindre
1024, le changement de la partition active, et de charger
autmatiquement le système par défaut après écoulement du délai prévu.
Il peut démarrer également des partitions Linux/BSD. Ce programme est
conçu pour installer et configurer mbldr sur le disque que vous
choisissez par des menus interactifs.

 Unité maître Erreur d'allocation mémoire Modèle n°: '%s' Plus d'une partition étendue a été trouvée. Aucune unité n'a été trouvée,  essayez d'indiquer explicitement l'unité avec l'option '-d' Aucune correspondance trouvée, errno=%i (%s) Auncune partition n'a été trouvée. Nombre de caractères disponibles pour le texte du menu de démarrage personnalisé: %i Nombre de caractères restants=%i Une seule unité adéquate a été trouvée, et sera utilisée:  Les options sont:
 -h Affiche un message d'aide, puis se termine
 -v Mode verbeux (debogage seulement, utilise un fichier journal)
 La partition a été ajoutée Interface primaire Terminer immédiatement Echec en lecture, errno=%i (%s) L'opération de lecture a retourné un nombre inattendu d'octets lus: %i Lecture du secteur %lu. Mémoire insuffisante, errno=%i (%s) Interface secondaire Taille d'un secteur: %u Devrait être 0xBEDD, valeur trouvée: 0x%X La taille d'un secteur est différente de 512. La taille de structure est égale soit à 74, soit à 66 La taille de structure est supérieure ou égale à 30 Taille de la structure retournée par l'interruption BIOS: %u Unité esclave La structure retournée par 'get device parameters' est plus petite que le minimum. Syntaxe:  Le nombre maximum de partitions est atteint (9). Vous ne pouvez plus en ajouter. Trop de caractères dans les libellés de partitions bootables.
L'installation de mbldr ne sera permise que lorsque vous aurez libéré au moins %i octets. Trop de caractères dans le texte du menu de démarrage personnalisé.
Recréez automatiquement un texte de menu ou chargez un fichier plus petit.
L'installation de mbldr ne sera permise que lorsque vous aurez libéré au moins %i octets. Trop de partitions ont été trouvées. Disque dur trop petit, il semble qu'une erreur soit apparue. Essai de l'unité de disque %s Je tente d'obtenir la description de l'unité %s Je tente de lire le modèle et le constucteur de l'unité %s Entrez le libellé pour cette partition:  Échec lors de la fermeture de l'unité %s, %s Impossible de fermer l'unité de disque %s Impossible de fermer le fichier temporaire, errno=%i (%s) Impossible de créer le fichier temporaire, errno=%i (%s) Impossible de supprimer le fichier temporaire, errno=%i, (%s) Impossible d'exécuter la commande sysctl Impossible de trouver le nombre maximum de périphériques Impossible d'ouvrir l'unité %s en lecture, %s Échec de l'ouverture de l'unité %s en écriture, %s Impossible d'ouvrir le fichier de description de l'unité, errno=%i, (%s) Impossible d'ouvrir le fichier de description du modèle, errno=%i, (%s) Impossible d'ouvrir le fichier de description du constructeur, errno=%i (%s) Echec de l'ouverture du fichier temporaire, errno=%i (%s) Échec lors de la lecture des données depuis l'unité %s, %s Impossible de lire le fichier de description de l'unité, errno=%i, (%s) Impossible de lire le fichier de description du modèle, errno=%i, (%s) Impossible de lire le fichier de description du constructeur, errno=%i (%s) Échec de lecture du secteur Impossible d'atteindre la position désirée, %s Échec d'écriture des données vers %s, %s Échec d'écriture du secteur Erreur inconnue, errno=%i (%s) Option inconnue %c Inconnu, ou non reconnu L'utilisateur a demandé de l'aide sur la ligne de commande. Que voulez-vous faire ? Windows NT NTFS ou OS/2 HPFS (IFS) L'opération d' écriture a retourné un nombre inattendu d'octets écrits: %i racine XENIX Appel système glob() réussi sur %s 