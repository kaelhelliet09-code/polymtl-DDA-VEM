import time

from dda_host import Bobine, Capteur, CarteCompetition, Direction

B1=Bobine.H4
B2=Bobine.H3    
B3=Bobine.H2
B4=Bobine.H1

C1=Capteur.CAPTEUR_4
C2=Capteur.CAPTEUR_3
C3=Capteur.CAPTEUR_1
C4=Capteur.CAPTEUR_2
#A enlever demarrerLancement(), et arereter 
#Budget de courant a respecter
#Augemtner le temps de lancement 
#Reduire a 2 grpahique sensopr + curant + filtrer courant. 
#enlever puissance et commande de bobine
#Nomenclature des bobines et capteurs a revoirs
#ligne pleine pour capteurs.
#Rajouter representatiojn graphique image speed gauge tout sur meme fig.
#Mode evaluatif.
def participant_code(carte: CarteCompetition):
    
    carte.demarrerLancement()
    carte.reglerCourant(3000)
    carte.activer(B1,Direction.AVANT)
    carte.attendreCapteur(C1)
    carte.desactiver(B1)
    # carte.activer(B2,Direction.AVANT)
    # carte.attendreCapteur(C2)
    # carte.desactiver(B2)
    carte.activer(B3,Direction.AVANT)
    carte.attendreCapteur(C3)
    carte.desactiver(B3)
    carte.activer(B4,Direction.AVANT)
    carte.attendreCapteur(C4)
    carte.desactiver(B4)
    ##code here :
    return carte.arreterLancement()
