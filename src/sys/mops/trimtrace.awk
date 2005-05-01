BEGIN { state = "<unknown??>"; depth = -1; }

{  if (state != $2 || depth != $3) {
     print;
     state = $2;
     depth = $3;
   }
} 
