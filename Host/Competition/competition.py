import time

from dda_host import Bobine, Capteur, CarteCompetition, Direction

B1=Bobine.H1
B2=Bobine.H2    
B3=Bobine.H3
B4=Bobine.H4

def participant_code(carte: CarteCompetition):
    carte.demarrerLancement()
    carte.reglerCourant(3000)
    ##################################
    carte.activer(B1, Direction.AVANT)
    carte.activer(B2, Direction.AVANT)


    ##################################
    carte.attendreCapteur(Capteur.CAPTEUR_1)
    carte.activer(B1, Direction.ARRIERE)
    #################################

    carte.attendreCapteur(Capteur.CAPTEUR_2)
    carte.desactiver(B1) 
    carte.activer(B2, Direction.ARRIERE)
    carte.activer(B3, Direction.AVANT)

    #################################
    carte.attendreCapteur(Capteur.CAPTEUR_3)
    carte.desactiver(B2)
    carte.activer(B3, Direction.ARRIERE)
    carte.activer(B4, Direction.AVANT)

    ###################################
    carte.attendreCapteur(Capteur.CAPTEUR_4)
    carte.activer(B4, Direction.ARRIERE)
    carte.desactiver(B3) 
    time.sleep(2)
    ###############################
    return carte.arreterLancement()
