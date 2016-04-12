typedef struct
 {
     String leader;
     String singular;
     String punctuation;
 }  message;

const short numberOpenStrings = 7;
const short numberClosedStrings = 6;

// Format: (message)+(number selected on dial)+" hours"+(punctuation)+" (Until ~"+closing_hour:minute+")"

const message messageOpenSpace[numberOpenStrings] {
  {"The space is open to members! Someone will be here for about ", "an hour", "!"},
  {"The space will be open (to members) for approximately ", "1 hour", "."},
  {"Got something to make? There's someone in the space for the next ", "hour or so", "."},
  {"Hackspace! Open to members! For approximately ", "an hour", "!"},
  {"Leeds Hackspace is open to members for around ", "an hour", "."},
  {"Get yourself down to the space and build things! Leeds Hackspace is open for ", "an hour", "."},
  {"Leeds Hackspace Members! There's someone at the space for at least the next ", "hour", "."}
};

const message messageCloseSpace[numberClosedStrings] {
  {"The space is closed."},
  {"The space is closed. You can get in if you're a keyholder."},
  {"Leeds Hackspace is currently empty."},
  {"There's apparently nobody here. The hackspace is closed."},
  {"The space is empty."},
  {"The hackspace is empty."}
  
};
