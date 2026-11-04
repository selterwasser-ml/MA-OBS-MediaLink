#include <cassert>
#include <string>
#include "../src/about_secret.h"
int main(){
    MAAboutSecret secret;
    assert(!secret.click("bela"));assert(!secret.click("bela"));
    assert(!secret.click("farin"));assert(secret.click("bela"));
    assert(!secret.click("bela")); // each opening consumes the sequence
    assert(!secret.click("farin"));assert(!secret.click("farin"));assert(secret.click("bela"));
    assert(!secret.click("farin"));assert(!secret.click("other"));assert(!secret.click("bela"));
    assert(!secret.click("farin"));secret.reset();assert(!secret.click("bela"));
}
