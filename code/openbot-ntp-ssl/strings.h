typedef struct
 {
     String leader;
     String singular;
     String punctuation;
 }  message;

const short numberOpenStrings = 6;
const short numberClosedStrings = 3;

// Format: (message)+(number selected on dial)+" hours"+(punctuation)+" (Until ~"+closing_hour:minute+")"

const message openSpace[numberOpenStrings] {
  {"The space is open to members! Someone will be here for about ", "an hour", "!"},
  {"The space will be open (to members) for approximately ", "1 hour", "."},
  {"There's someone in the space for the next ", "hour or so", "."},
  {"Hackspace! Open to members! For approximately ", "an hour", "!"},
  {"Leeds Hackspace is open to members for around ", "an hour", "."},
  {"Leeds Hackspace Members! There's someone at the space for at least the next ", "hour", "."}
};

const message closeSpace[numberClosedStrings] {
  {"The space is closed."},
  {"There's apparently nobody here. The hackspace is closed."},
  {"The hackspace is empty."}
};
