import time

from dda_host import Bobine, Capteur, CarteCompetition, Direction


def participant_code(carte: CarteCompetition):
    carte.demarrerLancement()
    carte.reglerCourant(3000)
    ##########
    carte.activer(Bobine.H1, Direction.AVANT)
    carte.activer(Bobine.H2, Direction.ARRIERE)
    #########
    carte.attendreCapteur(Capteur.CAPTEUR_1)
    ######### Après déclanchement du capteur 1 
    # carte.activer(Bobine.H1, Direction.ARRIERE)
    carte.desactiver(Bobine.H1)
    #########
    carte.attendreCapteur(Capteur.CAPTEUR_2)
    ########Après déclanchement du capteur 2
    carte.activer(Bobine.H2, Direction.AVANT)
    carte.activer(Bobine.H3, Direction.ARRIERE)
    ###########
    carte.attendreCapteur(Capteur.CAPTEUR_4)
    ############### Après déclanchement du capteur 3
    carte.activer(Bobine.H3, Direction.AVANT)
    carte.desactiver(Bobine.H2)
    carte.activer(Bobine.H4, Direction.AVANT)
    time.sleep(1)
    carte.desactiver(Bobine.H3)
    carte.desactiver(Bobine.H4)
    # #######
    return carte.arreterLancement()
