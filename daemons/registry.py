import subprocess

class DaemonRegistry:
    def __init__(self, sewer):
        self.sewer = sewer 
        # Mapping nicknames to the EXACT binary names in your build folder
        self.binaries = {
            "bebop":       "bebopd",
            "burned":      "burned",
            "turtlecom":   "turtlecomd",
            "granitor":    "granitord",
            "rocksteady":  "Rocksteady",
            "ratking":     "ratkingd",
            "metalhead":   "Metalheadd",
            "leatherhead": "leatherheadd",
            "splinter":    "splinterd",
            "krang":       "krangd"
        }

    def get_all_statuses(self):
        results = {}
        for nick, proc_name in self.binaries.items():
            try:
                # pgrep -f looks for the pattern in the full process command line
                subprocess.check_call(["pgrep", "-f", proc_name], 
                                     stdout=subprocess.DEVNULL, 
                                     stderr=subprocess.DEVNULL)
                results[nick] = "ACTIVE"
            except subprocess.CalledProcessError:
                results[nick] = "OFFLINE"
        return results
