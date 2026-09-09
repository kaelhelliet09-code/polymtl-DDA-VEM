import time

from dda_host import Bobine, Capteur, CarteCompetition, Direction

def participant_code(carte: CarteCompetition):
    carte.demarrerLancement()
    carte.reglerCourant(3000)
    ##################################
    carte.activer(Bobine.H1, Direction.AVANT)
    carte.activer(Bobine.H2, Direction.AVANT)
    ##################################
    carte.attendreCapteur(Capteur.CAPTEUR_1)
    ######### Après déclanchement du capteur 1 
    carte.activer(Bobine.H1, Direction.ARRIERE)
    #################################
    carte.attendreCapteur(Capteur.CAPTEUR_2)
    ########Après déclanchement du capteur 2
    carte.desactiver(Bobine.H1)
    carte.activer(Bobine.H2, Direction.ARRIERE)
    carte.activer(Bobine.H3, Direction.AVANT)
    #################################
    carte.attendreCapteur(Capteur.CAPTEUR_4)
    ############### Après déclanchement du capteur 4
    carte.activer(Bobine.H3, Direction.ARRIERE)
    carte.desactiver(Bobine.H2)
    carte.activer(Bobine.H4, Direction.AVANT)
    ############### Après déclanchement du capteur 3
    carte.attendreCapteur(Capteur.CAPTEUR_3)
    carte.activer(Bobine.H4, Direction.ARRIERE)
    carte.desactiver(Bobine.H3) 
    time.sleep(1)
    ###############################
    return carte.arreterLancement()
